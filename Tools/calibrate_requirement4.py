#!/usr/bin/env python3
"""通过 USART1 与示波器完成 G 题基本要求 4 的首轮采集。

每个频率依次下发 1.0 Vpp 到 2.0 Vpp 的 11 个 r4 命令。只有设备明确
回显 RX 和 OUT R4 后，才执行示波器 SINGLE 和波形采集。结果写到
artifacts/calibration/<round>.json；原始波形证据由 bench MCP 写入
artifacts/runs/。
"""

from __future__ import annotations

import asyncio
import json
import math
import os
import sys
import time
from pathlib import Path
from statistics import median
from typing import Any

import serial
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

from serial_tool import DEFAULT_BAUDRATE, DEFAULT_PORT, receive_for, send_command


ROOT = Path(__file__).resolve().parents[1]
SCOPE_RESOURCE = "USB0::0xF4EC::0x1011::SDS2PDDC5R0076::INSTR"
SCOPE_SERIAL = "SDS2PDDC5R0076"
FREQUENCIES_HZ = tuple(range(100, 3001, 100))
TARGETS_TENTHS_VPP = tuple(range(10, 21))
SCOPE_POINTS = 4000
ROUND_NAME = os.environ.get("CAL_ROUND", "serial_round1")
RESULT_PATH = ROOT / "artifacts" / "calibration" / f"{ROUND_NAME}.json"
POINT_LIMIT = int(os.environ.get("CAL_POINT_LIMIT", "0"))


def scope_vertical(target_tenths_vpp: int) -> tuple[float, float]:
    """在不削顶前提下取得尽可能高的垂直分辨率。"""
    if target_tenths_vpp <= 18:
        return 0.25, 0.0
    return 0.5, 0.0


def solve_3x3(matrix: list[list[float]], vector: list[float]) -> list[float]:
    """解 3 阶线性方程，用于 DC + sin + cos 最小二乘拟合。"""
    augmented = [list(row) + [value] for row, value in zip(matrix, vector)]
    for column in range(3):
        pivot = max(range(column, 3), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) < 1e-18:
            raise RuntimeError("正弦拟合矩阵奇异")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        divisor = augmented[column][column]
        augmented[column] = [value / divisor for value in augmented[column]]
        for row in range(3):
            if row == column:
                continue
            factor = augmented[row][column]
            augmented[row] = [left - factor * right
                               for left, right in zip(augmented[row],
                                                      augmented[column])]
    return [augmented[row][3] for row in range(3)]


def fit_vpp(manifest_path: str, frequency_hz: int) -> float:
    """从原始波形拟合已知频率的正弦幅值，低频时不依赖整周期窗口。"""
    run_path = Path(manifest_path).parent
    metadata = json.loads(
        (run_path / "scope" / "waveform_metadata.json").read_text(encoding="utf-8")
    )["metadata"]
    raw_codes = (run_path / "scope" / "waveform_codes.bin").read_bytes()
    volts_per_code = (metadata["vertical_scale_v_per_div"] /
                      metadata["codes_per_div"])
    offset_v = metadata["vertical_offset_v"]
    samples = [((code if code < 128 else code - 256) * volts_per_code) - offset_v
               for code in raw_codes]
    omega = 2.0 * math.pi * frequency_hz
    sample_interval_s = metadata["effective_sample_interval_s"]
    normal = [[0.0] * 3 for _ in range(3)]
    right = [0.0] * 3
    for index, value in enumerate(samples):
        basis = [1.0, math.sin(omega * index * sample_interval_s),
                 math.cos(omega * index * sample_interval_s)]
        for row in range(3):
            right[row] += basis[row] * value
            for column in range(3):
                normal[row][column] += basis[row] * basis[column]
    _, sine, cosine = solve_3x3(normal, right)
    return 2.0 * math.hypot(sine, cosine)


async def call_tool(session: ClientSession, tool: str,
                    arguments: dict[str, Any]) -> dict[str, Any]:
    result = await session.call_tool(tool, arguments)
    text = "\n".join(item.text for item in result.content if hasattr(item, "text"))
    if result.isError:
        raise RuntimeError(f"{tool} failed: {text}")
    return json.loads(text)


def write_result(records: list[dict[str, Any]]) -> None:
    by_frequency: dict[str, list[float]] = {}
    for record in records:
        ratio = record["measured_vpp"] / record["target_vpp"]
        by_frequency.setdefault(str(record["frequency_hz"]), []).append(ratio)
    payload = {
        "round": ROUND_NAME,
        "measurement": "CH1 after 10x amplifier and known model",
        "control": "USART1 COM6 r4 <Hz> <Vpp>",
        "records": records,
        "gain_correction_ratio_by_frequency": {
            frequency: median(ratios) for frequency, ratios in by_frequency.items()
        },
    }
    RESULT_PATH.parent.mkdir(parents=True, exist_ok=True)
    RESULT_PATH.write_text(json.dumps(payload, ensure_ascii=False, indent=2),
                           encoding="utf-8")


async def run() -> None:
    records: list[dict[str, Any]] = []
    params = StdioServerParameters(command=sys.executable,
                                   args=["-m", "econtest_bench_mcp"],
                                   cwd=str(ROOT))
    with serial.Serial(port=DEFAULT_PORT, baudrate=DEFAULT_BAUDRATE,
                       bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                       stopbits=serial.STOPBITS_ONE, timeout=0.05,
                       xonxoff=False, rtscts=False, dsrdtr=False) as uart:
        uart.dtr = False
        uart.rts = False
        receive_for(uart, 0.1)
        async with stdio_client(params) as (reader, writer):
            async with ClientSession(reader, writer) as session:
                await session.initialize()
                await call_tool(session, "scope_configure_edge_trigger", {
                    "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                    "source": "C1", "slope": "RISING", "level_v": 0.0,
                    "mode": "AUTO", "timeout_ms": 10000,
                })
                await call_tool(session, "scope_configure_timebase", {
                    "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                    "scale_s_per_div": 0.0002, "timeout_ms": 10000,
                })
                current_vertical: tuple[float, float] | None = None
                total = len(FREQUENCIES_HZ) * len(TARGETS_TENTHS_VPP)
                if POINT_LIMIT > 0:
                    total = min(total, POINT_LIMIT)
                point = 0
                for frequency_hz in FREQUENCIES_HZ:
                    for target_tenths_vpp in TARGETS_TENTHS_VPP:
                        if point >= total:
                            write_result(records)
                            return
                        point += 1
                        target_vpp = target_tenths_vpp / 10.0
                        command = f"r4 {frequency_hz} {target_vpp:.1f}"
                        response = send_command(uart, command, 0.15).decode(
                            "utf-8", errors="replace")
                        if ((f"RX {command}" not in response) or
                                (f"OUT R4 f={frequency_hz}Hz" not in response)):
                            raise RuntimeError(
                                f"USART1 command not acknowledged for {command}: {response!r}")

                        vertical = scope_vertical(target_tenths_vpp)
                        if vertical != current_vertical:
                            await call_tool(session, "scope_configure_channel", {
                                "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                                "channel": 1, "enabled": True, "coupling": "DC",
                                "probe_ratio": 1, "scale_v_per_div": vertical[0],
                                "offset_v": vertical[1], "timeout_ms": 10000,
                            })
                            current_vertical = vertical
                        await call_tool(session, "scope_control", {
                            "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                            "action": "SINGLE", "confirm_state_change": True,
                            "timeout_ms": 10000,
                        })
                        await asyncio.sleep(0.35)
                        status = await call_tool(session, "scope_status", {
                            "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                            "timeout_ms": 10000,
                        })
                        if status["status"]["trigger_status"]["raw"] != "Stop":
                            raise RuntimeError("示波器 SINGLE 未完成，采集终止")
                        captured = await call_tool(session, "scope_capture_waveform", {
                            "resource": SCOPE_RESOURCE, "serial": SCOPE_SERIAL,
                            "channel": 1, "points": SCOPE_POINTS,
                            "confirm_scope_stop": True, "timeout_ms": 30000,
                        })
                        measured_vpp = fit_vpp(captured["evidence"]["manifest"],
                                               frequency_hz)
                        record = {
                            "frequency_hz": frequency_hz,
                            "target_vpp": target_vpp,
                            "measured_vpp": measured_vpp,
                            "relative_error": (measured_vpp / target_vpp) - 1.0,
                            "scope_summary_vpp": captured["waveform"]["vpp_v"],
                            "evidence_manifest": captured["evidence"]["manifest"],
                        }
                        records.append(record)
                        write_result(records)
                        print(f"{point}/{total} {frequency_hz}Hz {target_vpp:.1f}Vpp "
                              f"=> {measured_vpp:.4f}Vpp", flush=True)

    write_result(records)


if __name__ == "__main__":
    try:
        asyncio.run(run())
    except Exception as exc:
        print(f"CALIBRATION FAILED: {exc}", file=sys.stderr)
        raise

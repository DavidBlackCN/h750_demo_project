#!/usr/bin/env python3
"""STM32 USART1 串口链路调试工具。

默认配置对应本工程 USART1：921600 baud、8N1、无流控。脚本既可用于
观察上电日志，也可发送 G 题控制命令并持续打印返回内容。

示例：
    python Tools/serial_tool.py --list
    python Tools/serial_tool.py --port COM8
    python Tools/serial_tool.py --port COM8 --command "r4 1000 2.0"

交互模式中直接输入命令并回车；输入 ``quit`` 或按 Ctrl+C 退出。
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Iterable

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    raise SystemExit(
        "缺少 pyserial。请先执行：python -m pip install pyserial"
    ) from exc


# 本工程当前 USB 转串口为 CH340；DapLink 仅用于 SWD 调试。
DEFAULT_PORT = "COM12"
DEFAULT_BAUDRATE = 921600
READ_TIMEOUT_S = 0.05


def print_ports() -> None:
    """打印系统中可用的串口，便于确认 DapLink VCOM 端口。"""
    ports = sorted(list_ports.comports(), key=lambda item: item.device)
    if not ports:
        print("未发现串口。请检查 DapLink USB 连接和驱动。")
        return

    for port in ports:
        description = port.description or "未知设备"
        hwid = port.hwid or ""
        print(f"{port.device:<8} {description}  {hwid}")


def print_received(data: bytes) -> None:
    """将设备返回的字节以 UTF-8 文本显示，保留不可解码字节。"""
    if data:
        print(data.decode("utf-8", errors="replace"), end="", flush=True)


def receive_for(port: serial.Serial, duration_s: float) -> bytes:
    """在指定时长内持续读取并显示 USART1 输出，返回原始响应字节。"""
    deadline = time.monotonic() + duration_s
    received = bytearray()
    while time.monotonic() < deadline:
        waiting = port.in_waiting
        data = port.read(waiting if waiting > 0 else 1)
        received.extend(data)
        print_received(data)
    return bytes(received)


def send_command(port: serial.Serial, command: str,
                 response_wait_s: float) -> bytes:
    """发送一条以 CRLF 结束的命令，打印并返回其后的设备响应。"""
    payload = command.encode("ascii") + b"\r\n"
    print(f">>> {command}")
    port.write(payload)
    port.flush()
    return receive_for(port, response_wait_s)


def run_interactive(port: serial.Serial, response_wait_s: float) -> None:
    """运行人工输入模式；每次回车即向设备发送一条完整命令。"""
    print("交互模式：输入 r2 <Hz>、r3 或 r4 <Hz> <Vpp>；quit 退出。")
    while True:
        try:
            command = input(">>> ").strip()
        except EOFError:
            print()
            return

        if command.lower() in {"quit", "exit"}:
            return
        if command:
            send_command(port, command, response_wait_s)


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="STM32H750 USART1（921600 8N1）调试工具"
    )
    parser.add_argument("--list", action="store_true", help="列出可用串口后退出")
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"串口名（默认 {DEFAULT_PORT}）")
    parser.add_argument(
        "--baudrate",
        type=int,
        default=DEFAULT_BAUDRATE,
        help=f"波特率（默认 {DEFAULT_BAUDRATE}）",
    )
    parser.add_argument(
        "--command",
        action="append",
        help="发送一条命令；可重复指定。未指定时进入交互模式。",
    )
    parser.add_argument(
        "--boot-wait",
        type=float,
        default=0.5,
        help="打开串口后先监听上电日志的秒数（默认 0.5）",
    )
    parser.add_argument(
        "--response-wait",
        type=float,
        default=1.0,
        help="每条命令发送后监听响应的秒数（默认 1.0）",
    )
    return parser.parse_args(argv)


def main(argv: Iterable[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.list:
        print_ports()
        return 0

    try:
        with serial.Serial(
            port=args.port,
            baudrate=args.baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=READ_TIMEOUT_S,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        ) as port:
            # DapLink VCOM 不需要 DTR/RTS；显式拉低，避免部分适配器错误复位目标板。
            port.dtr = False
            port.rts = False
            print(f"已打开 {port.port}，{port.baudrate} 8N1。")

            if args.boot_wait > 0.0:
                receive_for(port, args.boot_wait)

            if args.command:
                for command in args.command:
                    send_command(port, command, args.response_wait)
            else:
                run_interactive(port, args.response_wait)
    except serial.SerialException as exc:
        print(f"无法打开 {args.port}：{exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("\n已退出。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

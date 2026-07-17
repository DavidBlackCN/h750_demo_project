#!/usr/bin/env python3
"""按题目规定的 2～5 次谐波计算理想 THD。"""

import math


WAVE_ALIASES = {
    "正弦": "sine",
    "正弦波": "sine",
    "sine": "sine",
    "sin": "sine",
    "方波": "square",
    "square": "square",
    "三角": "triangle",
    "三角波": "triangle",
    "triangle": "triangle",
    "锯齿": "sawtooth",
    "锯齿波": "sawtooth",
    "saw": "sawtooth",
    "sawtooth": "sawtooth",
}


def harmonic_ratio(waveform: str, harmonic: int) -> float:
    """返回 Un/U1；幅值和 RMS 的公共比例在 THD 中相消。"""
    if waveform == "sine":
        return 0.0
    if waveform == "square":
        return 1.0 / harmonic if harmonic % 2 == 1 else 0.0
    if waveform == "triangle":
        return 1.0 / harmonic**2 if harmonic % 2 == 1 else 0.0
    if waveform == "sawtooth":
        return 1.0 / harmonic
    raise ValueError(f"不支持的波形：{waveform}")


def ideal_thd_to_fifth(waveform: str) -> float:
    return math.sqrt(sum(harmonic_ratio(waveform, n) ** 2 for n in range(2, 6)))


def read_positive_float(prompt: str) -> float:
    while True:
        try:
            value = float(input(prompt).strip())
            if value > 0.0:
                return value
        except ValueError:
            pass
        print("请输入大于 0 的数字。")


def main() -> None:
    print("可连续计算，输入 q 退出。\n")

    while True:
        wave_input = input("波形（正弦波/方波/三角波/锯齿波）：").strip().lower()
        if wave_input in {"q", "quit", "exit", "退出"}:
            return

        waveform = WAVE_ALIASES.get(wave_input)
        if waveform is None:
            print("不支持该波形。\n")
            continue

        read_positive_float("Vpp（V）：")
        actual_thd_percent = read_positive_float("实际THD（%）：")
        ideal_thd_percent = ideal_thd_to_fifth(waveform) * 100.0
        error_ratio = 1 - (ideal_thd_percent / actual_thd_percent)

        print(f"理想THD：{ideal_thd_percent:.6f} %")
        print(f"误差值（理想THD/实际THD）：{error_ratio:.6f}\n")


if __name__ == "__main__":
    main()

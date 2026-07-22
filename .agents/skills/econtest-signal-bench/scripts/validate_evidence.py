#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


def validate(manifest_path: Path) -> list[str]:
    errors: list[str] = []
    manifest_path = manifest_path.resolve()
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return [f"无法读取 manifest：{exc}"]
    root = manifest_path.parent
    if data.get("schema_version") != 1:
        errors.append("schema_version 缺失或不受支持")
    if not data.get("run_id"):
        errors.append("缺少 run_id")
    if data.get("complete") is True and data.get("status") != "complete":
        errors.append("complete=true 时 status 必须为 complete")
    files = data.get("files")
    if not isinstance(files, dict):
        errors.append("files 必须是对象")
        return errors
    for relative, expected in files.items():
        path = (root / relative).resolve()
        try:
            path.relative_to(root)
        except ValueError:
            errors.append(f"文件越出 run 目录：{relative}")
            continue
        if not path.is_file():
            errors.append(f"文件缺失：{relative}")
            continue
        payload = path.read_bytes()
        if expected.get("bytes") != len(payload):
            errors.append(f"字节数不匹配：{relative}")
        digest = hashlib.sha256(payload).hexdigest()
        if expected.get("sha256") != digest:
            errors.append(f"SHA-256 不匹配：{relative}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="校验电赛信号台证据 manifest。")
    parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    errors = validate(args.manifest)
    if errors:
        for error in errors:
            print(f"错误：{error}", file=sys.stderr)
        return 1
    print("有效")
    return 0


if __name__ == "__main__":
    sys.exit(main())

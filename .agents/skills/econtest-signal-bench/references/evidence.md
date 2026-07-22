# 证据解释

每次多步骤或写操作生成独立 run：

```text
artifacts/runs/<UTC时间>_<run_id>/
```

`manifest.json` 记录操作、策略、完成状态、结果和文件哈希；`events.jsonl` 记录关键状态转换。设备身份、配置回读、波形码、元数据和摘要使用受控相对路径。

## 判定

- `complete`：所有必需步骤和最终状态回读成功。
- `partial`：已有有用证据，但后续步骤失败。
- `inconclusive`：仪器返回无效值、采样窗口不足或数据不支持结论。

发送命令只是意图。只有设备回读能证明状态。输出关闭命令发送成功但 `OUTP?` 失败时，必须报告 unknown。

## 汇报

汇报型号、序列号、固件、关键设置、仪器测量、波形摘要、最终输出/示波器状态和 manifest 路径。不要把完整波形数组放入对话。

使用以下命令检查路径、大小和 SHA-256：

```powershell
python scripts/validate_evidence.py <manifest.json>
```

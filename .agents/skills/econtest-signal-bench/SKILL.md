---
name: econtest-signal-bench
description: 通过 econtest-bench-mcp 的 USB/VISA 工具安全控制 SIGLENT SDS2000X Plus 示波器和 SDG6000X/SDG6000X-E 信号发生器。用于电赛低压信号题中的设备发现、基本波产生、示波器配置、幅度/频率/时序测量、扫频、波形证据和完整回路验证。
---

# 电赛信号台

使用 MCP 工具访问硬件。以设备回读和证据文件为准，不把“命令已经发送”解释为“设置已经生效”。

## 首次访问前

用一句简短提醒确认：

- 信号源通道连接到哪里，示波器哪个通道观察哪里；
- 探头倍率、共地或隔离条件；
- 预期最大电压和频率范围；
- SDG 使用 `HZ` 还是 `50` 负载语义；
- 是否允许示波器 RUN/STOP/SINGLE 和信号源输出 ON。

线路、负载、电压、隔离、目标通道或任务范围实质变化时重新确认。任何写操作前读取 [safety.md](references/safety.md)。

## 设备选择

1. 调用 `bench_list_devices`。
2. 按型号和序列号确认唯一 scope 和 generator。
3. 后续尽量同时传入 `resource` 和 `serial`。
4. 多个同类设备时要求用户明确选择，不根据 USB 顺序猜测。
5. 用 `bench_status` 生成修改前基线。

## 支持的工具

只读：

- `bench_list_devices`
- `bench_identify`
- `scope_status`
- `generator_status`
- `bench_status`

信号源：

- `generator_configure_basic`
- `generator_set_load`
- `generator_set_output`
- `generator_configure_sweep`

示波器：

- `scope_configure_channel`
- `scope_configure_timebase`
- `scope_configure_edge_trigger`
- `scope_control`
- `scope_capture_waveform`

跨设备：

- `bench_capture_response`

读取 [device-support.md](references/device-support.md) 判断型号和具体能力的验证等级。

## 推荐完整回路流程

物理条件已确认时，优先调用 `bench_capture_response`，让服务器统一完成：

1. 校验两台仪器身份；
2. 确认信号源输出 OFF；
3. 配置信号源和示波器；
4. 使用确认门开启输出；
5. 等待、测量并采集波形；
6. 关闭输出并查询确认；
7. 保持示波器 STOP；
8. 返回摘要和 evidence manifest。

只有线路和授权范围与 `expected_destination` 一致时，才把 `confirm_output_enable` 设为 true。读取 [workflows.md](references/workflows.md) 获取分步流程和异常处理。

## 分步控制不变量

- 配置波形、负载和扫频时，目标输出必须回读为 OFF。
- 输出 ON 必须提供确认、非空目标说明和实际负载模式。
- 幅度不得超过服务器 3 Vpp 上限；不要尝试通过提示提高上限。
- 扫频关闭后重新配置所需基本波频率。
- 波形采集会将示波器置于 STOP；默认不恢复 RUN。
- 通信中断时，将输出最终状态报告为 unknown，并要求人工查看前面板。

## 证据解释

大型波形留在 `artifacts/runs/`，对话中只返回数值摘要、有效性、最终状态和路径。区分：

- `complete`：必需步骤和最终回读均成功；
- `partial`：已有可用文件，但后续步骤失败；
- `inconclusive`：测量无效或数据不足。

读取 [evidence.md](references/evidence.md)，并用 `scripts/validate_evidence.py` 校验完整证据。

## PNG 截图 TODO

当前固件上的 `PRIN? BMP/PNG` 通过 USB-VISA 超时。不要调用或虚构截图工具，不要把波形二进制文件说成 PNG。只有 MCP 工具列表出现经验证的截图工具且 [device-support.md](references/device-support.md) 已更新后，才使用截图能力。

## 禁止事项

- 禁止使用 LAN、无线、Telnet、WebControl 或 TCP 资源。
- 禁止任意 SCPI、`*RST`、固件、网络和仪器文件系统操作。
- 禁止在未确认目标、负载和电压时开启输出。
- 禁止在输出 ON 时修改基本波、负载或扫频。
- 禁止声称两个 USB 操作实现了硬件同步。
- 禁止把大波形数组粘贴到对话。

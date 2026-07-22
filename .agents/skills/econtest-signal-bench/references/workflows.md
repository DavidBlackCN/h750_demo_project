# 工作流

## 只读基线

1. `bench_list_devices` 枚举设备。
2. 用型号和序列号消除歧义。
3. `bench_status` 保存两台设备修改前状态。
4. 确认目标信号源通道为 OFF。

## 基本波分步流程

1. `generator_set_output(enabled=false)`。
2. `generator_set_load` 设置实际负载语义。
3. `generator_configure_basic` 配置波形。
4. 配置示波器通道、时基和边沿触发。
5. `scope_control(RUN)`。
6. 在当前授权范围内调用 `generator_set_output(enabled=true)`。
7. 幅度/频率验收调用 `scope_measure_simple`；波形形状诊断或留证调用 `scope_capture_waveform`。
8. 在 `finally` 语义下关闭信号源输出。
9. 查询确认 OFF；示波器最终保持 STOP。

## 推荐完整回路

优先使用 `bench_capture_response`。它只处理一次基本波响应，自动生成统一证据，并在异常时重新打开设备请求 OFF/STOP。

如果返回的最终输出状态为 unknown，立即要求操作者查看信号源前面板，不得继续自动测试。

## 扫频

1. 输出 OFF 时先配置 SINE、SQUARE 或 RAMP 载波。
2. 调用 `generator_configure_sweep(enabled=true)`。
3. 经确认后开启输出并执行观测。
4. 关闭输出。
5. 关闭扫频。
6. 再次配置固定基本波，因为实机关闭扫频后可能保留瞬时载波频率。

## SINGLE 超时

SINGLE 在无触发条件时可能长期保持 Ready。为 `scope_control` 设置有界 `single_wait_timeout_s`；只有返回 `single.completed=true` 才是已验证的单帧。超时、Ready 或原生测量无效时，结果为 `inconclusive`，不得伪装为触发成功或更新校准表。超时后调用 STOP 并回读。

## 外部 DUT 流程

适用于 `PC/串口 → MCU/AD9910 → DUT → SDS`，而不是 SDG 完整回路：

1. 调用方自行发送 DUT 命令并接收可解析回显；记录命令发送和回显完成时间。
2. 从回显完成后开始等待调用方显式指定的稳定时间。
3. 需要新帧时执行 `scope_control(SINGLE)` 并确认 `single.completed=true`。
4. 用 `scope_measure_simple(PKPK, FREQ, RMS)` 读取原生值，可设置多次读取；保存同一个 `measurement_point_id` 和 `dut_context`。
5. 用 `scope_capture_waveform` 保存同测点的波形证据。波形摘要不能写入增益表。
6. STOP 并确认。DUT 输出的关闭或保持状态只由其外部控制器记录。

## 批量校准门槛

先以 1 至 3 个代表点比较示波器原生 `PKPK` 与导出波形摘要的 Vpp。未达到项目指定的一致性门限，或任一测点为 `inconclusive` 时，停止批量执行。整批任务以原生读数的稳健统计量更新一个增益；复测仍使用同一原生方法。

## PNG 截图 TODO

当前不要尝试截图。`PRIN?` 在已验证固件上超时；只使用 `scope_capture_waveform` 保存原始波形证据。

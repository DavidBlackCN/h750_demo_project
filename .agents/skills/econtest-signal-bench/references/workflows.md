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
7. 测量或调用 `scope_capture_waveform`。
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

SINGLE 在无触发条件时可能长期保持 Ready。使用有界等待；超时后调用 STOP 并回读。超时是有效结果，不应伪装为触发成功。

## PNG 截图 TODO

当前不要尝试截图。`PRIN?` 在已验证固件上超时；只使用 `scope_capture_waveform` 保存原始波形证据。

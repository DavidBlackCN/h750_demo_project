# 项目状态

更新时间：2026-07-17

## 已验证

- `cmake --build --preset Debug` 编译、链接通过。
- 当前构建后大致资源占用：
  - FLASH：85292 B / 128 KB（65.07%）。
  - DTCMRAM：27864 B / 128 KB（21.26%）。
  - RAM_D2：4096 B / 288 KB（1.39%）。
- CubeMX ADC 配置已接入：PA1_C / ADC1_INP1、16 bit、8.5 cycles、DMA1 Stream0 one-shot/high priority。当前 TIM1 时钟为 75 MHz；针对固定 1 kHz 输入，功能层运行时设置 Prescaler=0、Period=2416，采样率约 31030.2027 Hz。
- ADC/FFT/THD/波形识别代码已编译、链接通过。10 kHz、1 Vpp 三角波优化前实测为 `freq=9978.26Hz thd=11.584% h2=0.0009 h3=0.1088 h5=0.0397`，相对题目五次谐波口径的理想 THD 11.809182% 偏低约 0.225 个百分点；连续频率细化版本仍需复测。
- CubeMX 测频配置已接入：PA0 / TIM2_CH1、32 位满量程计数、DMA1 Stream5、TIM2 和 DMA 中断优先级 2。
- 方波测频代码已编译、链接通过；1 Hz 到 1 MHz 范围和 0.5 Hz/10 Hz 输出分辨率仍需信号源实测确认。
- DAC8830 驱动已接入构建，新增文件：
  - `HDL/DAC8830.h`
  - `HDL/DAC8830.c`
  - `HDL/DAC8830_DMA.h`
  - `HDL/DAC8830_DMA.c`
  - `HDL/DAC8830_SPI.h`
  - `HDL/DAC8830_SPI.c`

## 当前主程序行为

- 启动后只初始化 GPIO、DMA、ADC1、TIM1 和 USART1，并只运行 ADC/FFT/THD demo。
- ADC 启动时执行单端 offset 校准，并在功能层显式设置 TIM1 Update TRGO；DMA 每帧采集 1024 点。
- DMA 完成后停止采样、维护 DCache、换算电压、去直流并执行 Hann 窗 FFT。
- USART1 严格按原始波形、512 点半边频谱、频率/THD/波形结果的顺序输出；上电只采集并打印一帧，完成后主循环保持空闲，不自动重启。
- 半边频谱每行仅输出 `bin,幅值V`，不再输出每个 bin 对应的频率；测得的输入信号频率只在最终 `result freq=...Hz` 中输出。
- 单帧采集超过 100 ms 未完成时输出 `adc capture timeout` 并停止采集，不自动重试。
- 实测日志曾停在 `adc init begin fs=500000.00Hz`，确认原 Period=149 只产生 500 kSPS，且启动校准路径阻塞。当前改为运行时配置约 31.0302 kSPS，并暂时跳过启动 offset 校准；FFT 前去均值可消除直流偏置。DMA 满帧回调立即停 TIM1，避免 one-shot DMA 后续触发造成 overrun。
- 跳过校准后日志停在 `adc timer configured, starting dma`；已移除首帧启动前不必要的 `HAL_ADC_Stop_DMA()`，并增加 cache、ADC DMA、TIM1 三阶段首帧日志，便于确认硬件启动卡点。
- 后续日志确认停在 `adc cache prepare`，尚未进入 `HAL_ADC_Start_DMA()`。当前工程未调用 `SCB_EnableDCache()`，现已将 ADC DMA 缓冲区的 clean/invalidate 改为仅在 `SCB->CCR.DC` 开启时执行，避免未启用 DCache 时维护操作触发 Fault。
- THD 严格按题目口径统计 Nyquist 范围内的 2～5 次谐波；当前 ADC demo 在 FFT 主峰附近细化连续频率，并直接计算基波及各次谐波频点的 Hann 加窗 DFT，减小固定三点积分对不同小数 bin 位置产生的主瓣截断误差。波形识别结果为 `sine`、`square`、`triangle`、`unknown` 或 `none`。
- 当前主程序不启动 TIM2 方波测频、AD9833、DAC8830、片上 DAC1 或 SPI1；相关模块代码继续保留。

## 已实现模块

- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移，底层使用 SPI1 硬件发送。
- `HDL/DAC8830_DMA.*`：TIM4 + DMA 波形输出，DMA1 Stream2 拉低 CS、Stream3 写 SPI1 TXDR、Stream4 拉高 CS。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：ADC DMA 缓冲、回调状态和调试信息。
- `FML/FFT_FML.*`：1024 点 FFT、Hann 窗、正确的线性幅值缩放、峰值和相位基础计算。
- `FML/FREQ_FML.*`：TIM2 输入捕获自动量程、低频中断、高频 DMA、DCache 维护、超时和频率结果。
- `BLL/FFT_BLL.*`：FFT 结果、主峰连续频率细化、谐波频点幅值、THD 和波形识别；保留原频谱积分入口供未提供时域数据的旧调用使用。
- `API/ADC_API.*`：ADC 单帧状态流程、FFT/THD 分析以及原始波形、半边频谱和结果输出。
- `API/FFT_API.*`：独立 FFT 输出入口，当前主循环未直接调用。
- `API/FREQ_API.*`：测频初始化、主循环处理、结果访问和 USART1 摘要输出入口。

## 当前引脚定义

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| USART1_TX | PB6 | 串口调试输出，921600 baud |
| USART1_RX | PB7 | 串口调试输入，921600 baud |
| FREQ_IN / TIM2_CH1 | PA0 | 方波测频输入，上升沿，0 到 3.3 V |
| ADC1_INP1 | PA1_C | ADC1 采样输入 |
| DAC1_OUT1 | PA4 | 片上 DAC1 CH1 输出 |
| AD9833_CS / FSYNC | PA1 | AD9833 软件 SPI 片选 |
| AD9833_SDA | PH4 | AD9833 软件 SPI 数据 |
| AD9833_SCK | PH5 | AD9833 软件 SPI 时钟 |
| AD9910_MRT | PA6 | AD9910 master reset |
| AD9910_CSN | PD4 | AD9910 串行控制片选 |
| AD9910_SCK | PA8 | AD9910 串行控制时钟 |
| AD9910_SDI | PA12 | AD9910 串行控制数据 |
| AD9910_IUP | PD5 | AD9910 IO update |
| AD9910_PF0 | PG11 | AD9910 Profile 选择位 0 |
| AD9910_PF1 | PG9 | AD9910 Profile 选择位 1 |
| AD9910_PF2 | PG7 | AD9910 Profile 选择位 2 |
| DAC8830_CS1 | PE2 | DAC8830 通道 A 片选 |
| DAC8830_CS2 | PE0 | DAC8830 通道 B 片选 |
| DAC8830_SDI | PA7 / SPI1_MOSI | DAC8830 硬件 SPI 数据 |
| DAC8830_SCLK | PA5 / SPI1_SCK | DAC8830 硬件 SPI 时钟 |

注意：`AD9833_CS / FSYNC` 当前使用 PA1，而 ADC1 输入也配置为 PA1_C。H750 的 PA1/PA1_C 模拟开关配置需要结合实物接线确认，若两者同时使用需优先排查引脚冲突。

## 待确认

- 方波测频尚未用实物信号源覆盖验证 1 Hz、频段切换点、10 kHz 分辨率切换点和 1 MHz 上限；应重点观察串口 `raw`、`mode`、`ticks` 和 `periods`。
- 当前 TIM2 标称时钟为 75 MHz，来自内部 HSI。已按本次 1 MHz 实测做单点比例校准，但系数可能随板卡、温度和时钟条件变化；高精度场景仍建议多频点复核或使用外部高精度时钟。
- PA0 输入必须限制在 0 到 3.3 V，并与信号源共地；5 V 方波不可直接输入。
- ADC 输入必须限制在 0 到 3.3 V；双极性信号需要外部偏置/调理，推荐先用 2 Vpp、1.65 V offset 测试。
- `.ioc` 中 TIM1 已配置 Update TRGO，但 CubeMX 生成值可能与运行值不同；功能层已在启动时覆盖成 Update 和 Period=2416。若需同步 CubeMX 配置，请手动将 TIM1 Period 改为 2416 并检查生成结果。
- 当前约 31.0302 kSPS、1024 点配置专用于固定 1 kHz 输入：单帧约 33.000107 个周期，接近相干采样；FFT bin 间隔约 30.303 Hz，五次谐波低于 Nyquist。输入频率变化后需要重新选择采样率。
- THD 已限制为题目规定的 2～5 次谐波，避免更高次谐波、噪声和混叠抬高结果；连续频率细化算法的离线 10 kHz 理想三角波结果约为 11.847%，但 ADC、信号源失真、采样时钟误差和单帧噪声仍会影响实测，需复测本次 10 kHz/1 Vpp 条件，并继续覆盖不同相位、幅值和频率。
- AD9910 驱动已有基础函数，但主程序当前没有调用，硬件链路和寄存器参数仍需实测。
- DAC8830 模块的软件换算固定为 ±10 V，原正弦 demo 参数为 10 kHz、1 Vpp、250 点；该 demo 当前不由主程序启动。
- SPI1 初始化已配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/2 分频；当前 SPI123 时钟改用 PLL3，目标 120 MHz，SCK 约 60 Mbit/s。
- DAC8830 正弦模块使用 250 点表和 TIM4 DMA 定时，目标更新率 2.5 MS/s；该链路当前不运行，重新启用后仍需用示波器或逻辑分析仪确认 10 kHz 输出和阶梯/毛刺情况。

## 工作区注意

- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

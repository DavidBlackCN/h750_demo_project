# 项目状态

更新时间：2026-07-16

## 已验证

- `cmake --build --preset Debug` 编译、链接通过。
- 当前构建后大致资源占用：
  - FLASH：58524 B / 128 KB。
  - DTCMRAM：3040 B / 128 KB。
  - RAM_D2：100096 B / 288 KB。
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

- 启动后通过 USART1 输出状态日志。
- 只初始化 GPIO、DMA、USART1 和 TIM2，并只运行方波测频 demo。
- PA0 / TIM2_CH1 启动后先做约 10 ms 频段探测。
- 低频使用 DIV1 输入捕获中断并累计至少约 200 ms；高频使用 DIV8 输入捕获 DMA，目标测量窗口约 200 ms，以减小输入边沿抖动造成的随机误差。
- 低于 10 kHz 的有效结果按 0.5 Hz 输出，10 kHz 到 1 MHz 结果按 10 Hz 输出；无信号约 2.2 s 后报告 `no_signal`。
- 根据 1 MHz 输入实测为 1002814 Hz 的结果加入 `0.997193896` 校准系数；校准后 1 MHz 附近轻微超限会钳位显示为 1000000 Hz，不再输出 0。超过 1.01 MHz 时保留 `above_range` 状态并显示量程上限。
- USART1 每 1 s 持续输出显示频率、原始频率、IC/DMA 模式、状态、tick 和累计周期数；结果不变或无信号时也继续输出。
- 已继续优化高频扫频时偶发串口停止问题：高频 DMA 模式关闭 TIM2 和 DMA1 Stream5 NVIC，主循环轮询 DMA NDTR，并在超时后主动中止和重新探测，避免 HAL DMA 完成/中止/立即重启的中断竞态；切回低频模式时恢复 TIM2 NVIC。该修复已编译通过，仍需实物复测。
- 当前主程序不启动 AD9833、DAC8830、片上 DAC1、ADC DMA 和 FFT 处理链路。

## 已实现模块

- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移，底层使用 SPI1 硬件发送。
- `HDL/DAC8830_DMA.*`：TIM4 + DMA 波形输出，DMA1 Stream2 拉低 CS、Stream3 写 SPI1 TXDR、Stream4 拉高 CS。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：ADC DMA 缓冲、回调状态和调试信息。
- `FML/FFT_FML.*`：1024 点 FFT、窗函数、峰值和相位基础计算。
- `FML/FREQ_FML.*`：TIM2 输入捕获自动量程、低频中断、高频 DMA、DCache 维护、超时和频率结果。
- `BLL/FFT_BLL.*`：FFT 结果结构体和主峰/次峰插值频率。
- `API/ADC_API.*`：ADC 数据处理、FFT 计算和串口输出入口。
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

- ADC1 当前配置为 `ADC_EXTERNALTRIG_T1_TRGO`，但 `MX_TIM1_Init()` 中 TIM1 TRGO 是 `TIM_TRGO_RESET`。如果实测没有 ADC DMA 回调，应优先把 TIM1 TRGO 改为 update 或改用软件触发。
- `adc1_deal()` 处理完一帧后调用 `HAL_ADC_Stop_DMA()`，当前没有自动重启。若需要连续采样，需要补重启逻辑或改 DMA circular。
- FFT 标定目前按 1 MHz 采样率常量 `FFT_SAMPLE_RATE` 计算频率，实测前需要确认 TIM1/ADC 真实采样率。
- AD9910 驱动已有基础函数，但主程序当前没有调用，硬件链路和寄存器参数仍需实测。
- DAC8830 模块的软件换算固定为 ±10 V，原正弦 demo 参数为 10 kHz、1 Vpp、250 点；该 demo 当前不由主程序启动。
- SPI1 初始化已配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/2 分频；当前 SPI123 时钟改用 PLL3，目标 120 MHz，SCK 约 60 Mbit/s。
- DAC8830 正弦模块使用 250 点表和 TIM4 DMA 定时，目标更新率 2.5 MS/s；该链路当前不运行，重新启用后仍需用示波器或逻辑分析仪确认 10 kHz 输出和阶梯/毛刺情况。

## 工作区注意

- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

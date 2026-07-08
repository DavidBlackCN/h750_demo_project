# 项目状态

更新时间：2026-07-08

## 已验证

- `cmake --build --preset Debug` 编译、链接通过。
- 当前构建后大致资源占用：
  - FLASH：约 86 KB / 128 KB。
  - DTCMRAM：约 27 KB / 128 KB。
  - RAM_D2：约 9 KB / 288 KB。
- DAC8830 驱动已接入构建，新增文件：
  - `HDL/DAC8830.h`
  - `HDL/DAC8830.c`
  - `HDL/DAC8830_SPI.h`
  - `HDL/DAC8830_SPI.c`

## 当前主程序行为

- 启动后通过 USART1 输出状态日志。
- AD9833 初始化为 1 kHz 正弦。
- 片上 DAC1 输出 1 kHz、1.00 Vpp、1.65 V 偏置正弦。
- ADC1 校准后启动 DMA 采样。
- 主循环处理一帧 ADC 数据，计算 FFT，并通过 USART1 输出原始数据、频谱和摘要。

## 已实现模块

- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：ADC DMA 缓冲、回调状态和调试信息。
- `FML/FFT_FML.*`：1024 点 FFT、窗函数、峰值和相位基础计算。
- `BLL/FFT_BLL.*`：FFT 结果结构体和主峰/次峰插值频率。
- `API/ADC_API.*`：ADC 数据处理、FFT 计算和串口输出入口。
- `API/FFT_API.*`：独立 FFT 输出入口，当前主循环未直接调用。

## 当前引脚定义

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| USART1_TX | PB6 | 串口调试输出，921600 baud |
| USART1_RX | PB7 | 串口调试输入，921600 baud |
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
| DAC8830_SDI | PA7 | DAC8830 软件 SPI 数据 |
| DAC8830_SCLK | PA5 | DAC8830 软件 SPI 时钟 |

注意：`AD9833_CS / FSYNC` 当前使用 PA1，而 ADC1 输入也配置为 PA1_C。H750 的 PA1/PA1_C 模拟开关配置需要结合实物接线确认，若两者同时使用需优先排查引脚冲突。

## 待确认

- ADC1 当前配置为 `ADC_EXTERNALTRIG_T1_TRGO`，但 `MX_TIM1_Init()` 中 TIM1 TRGO 是 `TIM_TRGO_RESET`。如果实测没有 ADC DMA 回调，应优先把 TIM1 TRGO 改为 update 或改用软件触发。
- `adc1_deal()` 处理完一帧后调用 `HAL_ADC_Stop_DMA()`，当前没有自动重启。若需要连续采样，需要补重启逻辑或改 DMA circular。
- FFT 标定目前按 1 MHz 采样率常量 `FFT_SAMPLE_RATE` 计算频率，实测前需要确认 TIM1/ADC 真实采样率。
- AD9910 驱动已有基础函数，但主程序当前没有调用，硬件链路和寄存器参数仍需实测。
- DAC8830 驱动已编译通过，但还没有在 H750 硬件上确认时序和输出量程。

## 工作区注意

- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

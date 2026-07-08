# STM32H750 电赛信号题模板项目

这是一个面向电子设计竞赛信号题的 STM32H750 模板工程，目标是把常用的信号产生、采集、频谱分析和串口调试能力提前搭好，后续按题目要求快速组合业务逻辑。

## 当前能力

- AD9833 软件 SPI 驱动：默认启动输出 1 kHz 正弦波。
- AD9910 基础驱动：包含 GPIO 初始化、串行写寄存器、单 Profile/并行 Profile 初始化与 Profile 选择。
- DAC8830 驱动：已移植高层电压/码值接口和 H750 HAL GPIO 软件 SPI 底层。
- STM32H750 片上 DAC 波形输出：TIM4 触发 DAC1 CH1 DMA，支持正弦、方波、三角波、锯齿波、直流。
- ADC1 + DMA 采样框架：ADC1 INP1，DMA 缓冲区放在 `.dma_buffer`，供 FFT 使用。
- CMSIS-DSP FFT：1024 点 FFT，支持汉宁窗、主峰/次峰、频率、幅值、Vpp、相位估计。
- USART1 调试输出：921600 8N1，主要用于输出启动状态、原始采样和频谱数据。

## 项目结构

- `Core/`：STM32CubeMX 生成的 HAL 初始化代码和 `main.c`。
- `HDL/`：硬件驱动层，放外部芯片和软件 SPI 等底层驱动。
- `FML/`：功能模块层，封装 ADC、DAC、FFT、USART 等可复用功能。
- `BLL/`：业务逻辑层，放采样数据转换、FFT 结果整理、串口批量发送等逻辑。
- `API/`：应用入口层，当前主循环调用 `adc_proc()` 完成采样帧处理和 FFT 打印。
- `Drivers/`、`Middlewares/`：STM32 HAL、CMSIS 和 DSP 相关依赖。
- `cmake/`、`CMakeLists.txt`：CMake/Ninja 构建配置。

## 默认启动流程

`Core/Src/main.c` 当前流程：

1. 初始化 MPU、HAL、时钟和 CubeMX 外设。
2. USART1 打印 `boot`。
3. 初始化 AD9833，并输出 1 kHz 正弦。
4. 启动片上 DAC1，输出 1 kHz、1.00 Vpp、1.65 V 偏置正弦。
5. 校准 ADC1，启动 ADC DMA。
6. 主循环调用 `adc_proc()`，处理采样、FFT 和串口输出。

## 构建

推荐使用 CMake Preset：

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

当前已验证 `cmake --build --preset Debug` 可以通过。

## 关键配置

- MCU：STM32H750XBHx，工程名 `adc_fft_demo`。
- USART1：PB6 TX、PB7 RX，921600 baud。
- ADC1：PA1_C / ADC1_INP1，16 bit，DMA one-shot。
- 片上 DAC1：PA4 / DAC1_OUT1，TIM4 TRGO 触发，DMA circular。
- AD9833：`CS=PA1`、`SDA=PH4`、`SCK=PH5`。
- AD9910：见 `Core/Inc/main.h` 中 `MRT/PF0/PF1/PF2/IUP/CSN/SDI/SCK9` 宏。
- DAC8830：默认 `CS1=PE2`、`CS2=PE0`、`SDI=PA7`、`SCLK=PA5`。

更多接线和开发注意事项见 [GUIDE.md](GUIDE.md)，当前状态见 [STATUS.md](STATUS.md)。

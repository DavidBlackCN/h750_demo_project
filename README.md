# STM32H750 电赛信号题模板项目

这是一个面向电子设计竞赛信号题的 STM32H750 模板工程，目标是把常用的信号产生、采集、频谱分析和串口调试能力提前搭好，后续按题目要求快速组合业务逻辑。

## 当前能力

- AD9833 软件 SPI 驱动：支持寄存器、频率、相位和波形控制。
- AD9910 基础驱动：包含 GPIO 初始化、串行写寄存器、单 Profile/并行 Profile 初始化与 Profile 选择。
- DAC8830 驱动：已移植高层电压/码值接口，底层使用 SPI1 硬件发送；保留 TIM4 + DMA 驱动 SPI1 输出 ±10 V 模式 10 kHz、1 Vpp、250 点正弦的 demo。
- STM32H750 片上 DAC 波形输出：TIM4 触发 DAC1 CH1 DMA，支持正弦、方波、三角波、锯齿波、直流。
- ADC1 + DMA 采样：PA1_C / ADC1_INP1，当前针对固定 1 kHz 输入将 TIM1 TRGO 配置为约 31.0302 kSPS，单帧 1024 点接近相干采样，DMA 缓冲区位于 `.dma_buffer`。
- CMSIS-DSP FFT 分析：1024 点 Hann 窗 FFT，输出原始波形和 512 点半边频谱；主峰使用连续频率细化，并在细化后的基波及 2～5 次谐波频点直接估计幅值，计算题目口径的 THD、谐波比例及波形识别结果。
- 方波频率测量：PA0 / TIM2_CH1 输入捕获，覆盖 1 Hz 到 1 MHz；低频使用捕获中断，高频使用 `/8` 捕获 DMA，输出分辨率分别为 0.5 Hz 和 10 Hz。
- USART1 调试输出：921600 8N1，主要用于输出启动状态、原始采样和频谱数据。

## 项目结构

- `Core/`：STM32CubeMX 生成的 HAL 初始化代码和 `main.c`。
- `HDL/`：硬件驱动层，放外部芯片和软件 SPI 等底层驱动。
- `FML/`：功能模块层，封装 ADC、DAC、FFT、测频、USART 等可复用功能。
- `BLL/`：业务逻辑层，放采样数据转换、FFT 结果整理、串口批量发送等逻辑。
- `API/`：应用入口层，放测频、采样帧处理和 FFT 打印等应用入口。
- `Drivers/`、`Middlewares/`：STM32 HAL、CMSIS 和 DSP 相关依赖。
- `cmake/`、`CMakeLists.txt`：CMake/Ninja 构建配置。

## 默认启动流程

`Core/Src/main.c` 当前流程：

1. 初始化 MPU、HAL、系统时钟、GPIO、DMA、ADC1、TIM1 和 USART1。
2. USART1 打印 `boot`。
3. `ADC_API_Init()` 校准 ADC，并启动 1024 点 DMA 单帧采集。
4. 主循环只运行 `ADC_API_Process()`，依次完成去直流、FFT、频率、THD 和波形识别。
5. USART1 依次输出原始波形、半边频谱和分析结果；上电只采集并打印一次，完成后主循环保持空闲。

当前主任务只运行 ADC/FFT/THD demo，不启动方波输入捕获、AD9833、DAC8830 或片上 DAC；这些模块仍保留在工程中供后续题目组合。

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
- 方波测频：PA0 / TIM2_CH1，上升沿输入捕获；TIM2 时钟当前为 75 MHz，DMA 使用 DMA1 Stream5。
- ADC1：PA1_C / ADC1_INP1，16 bit、8.5 cycles、DMA one-shot；TIM1 Update TRGO 使用 2417 个定时器计数，75 MHz 时采样率约为 31030.2027 Hz。
- 片上 DAC1：PA4 / DAC1_OUT1，TIM4 TRGO 触发，DMA circular。
- AD9833：`CS=PA1`、`SDA=PH4`、`SCK=PH5`。
- AD9910：见 `Core/Inc/main.h` 中 `MRT/PF0/PF1/PF2/IUP/CSN/SDI/SCK9` 宏。
- DAC8830：默认 `CS1=PE2`、`CS2=PE0`、`SDI=PA7/SPI1_MOSI`、`SCLK=PA5/SPI1_SCK`；当前 DMA demo 使用 TIM4 产生 2.5 MS/s 更新节拍。

更多接线和开发注意事项见 [GUIDE.md](GUIDE.md)，当前状态见 [STATUS.md](STATUS.md)。

# STM32H750 电赛信号题模板项目

这是一个面向电子设计竞赛信号题的 STM32H750 模板工程，目标是把常用的信号产生、采集、频谱分析和串口调试能力提前搭好，后续按题目要求快速组合业务逻辑。

## 当前能力

- AD9833 软件 SPI 驱动：支持寄存器、频率、相位和波形控制。
- AD9910 基础驱动：包含 GPIO 初始化、串行写寄存器、单 Profile/并行 Profile 初始化与 Profile 选择。
- DAC8830 驱动：已移植高层电压/码值接口，底层使用 SPI1 硬件发送；当前主程序用 TIM4 + DMA 驱动 SPI1 输出 ±10 V 模式 10 kHz、1 Vpp、250 点正弦。
- STM32H750 片上 DAC 波形输出：TIM4 触发 DAC1 CH1 DMA，支持正弦、方波、三角波、锯齿波、直流。
- ADC1 + ADC2 双重规则同步采样：频率测量帧以1 MS/s同时触发`PA1_C / ADC1_INP1`和`PA7 / ADC2_INP7`，相位帧根据测得频率自动调整TIM1分频和采样点数。
- CMSIS-DSP FFT：4096 点 FFT，支持汉宁窗、主峰/次峰、频率、幅值、Vpp、相位估计。
- 双正弦相位差：覆盖 1 kHz 到 100 kHz，FFT 粗定位后使用公共频率正弦拟合，并预留多频点相位校准表。
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

1. 将CPU提升到480 MHz并开启I/D Cache，AXI/AHB为240 MHz，APB和外设定时器时钟分别为120 MHz和240 MHz。
2. 以1 MS/s采集4096点频率测量帧，通过FFT和正弦拟合测出两路公共频率。
3. 根据频率联合搜索TIM1分频、3072～4096点采样长度和整数周期数，采集第二帧相干相位数据。
4. 使用已测公共频率拟合两路相位，输出相位差、实际采样率、点数、周期数、闭合误差和有效标志。
5. 恢复1 MS/s并重新测频，循环执行“测频帧→相干相位帧”。

当前主任务只运行双通道正弦相位差 demo，不启动方波测频、AD9833、AD9910、DAC8830或片上 DAC；这些模块仍保留供后续题目组合。

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
- ADC1：PA1_C / ADC1_INP1，双 ADC 主机通道。
- ADC2：PA7 / ADC2_INP7，双 ADC 从机通道；与DAC8830的SPI1_MOSI复用，不能同时运行。
- CPU：480 MHz；AXI/AHB：240 MHz；TIM1计数时钟：240 MHz。
- ADC 采样：16 bit、TIM1 TRGO、DMA normal；测频帧固定4096点/1 MS/s，相位帧动态3072～4096点、采样率1～1.1 MS/s。
- 片上 DAC1：PA4 / DAC1_OUT1，TIM4 TRGO 触发，DMA circular。
- AD9833：`CS=PA1`、`SDA=PH4`、`SCK=PH5`。
- AD9910：见 `Core/Inc/main.h` 中 `MRT/PF0/PF1/PF2/IUP/CSN/SDI/SCK9` 宏。
- DAC8830：默认 `CS1=PE2`、`CS2=PE0`、`SDI=PA7/SPI1_MOSI`、`SCLK=PA5/SPI1_SCK`；当前 DMA demo 使用 TIM4 产生 2.5 MS/s 更新节拍。

更多接线和开发注意事项见 [GUIDE.md](GUIDE.md)，当前状态见 [STATUS.md](STATUS.md)。

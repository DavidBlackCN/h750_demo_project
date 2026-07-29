# STM32H750 电赛信号题模板项目

当前主任务为 ADC1 采集、FFT 与谐波分量测量：按下 `PC4`（内部上拉、按下接地）启动一帧 ADC1 采集，TIM1_TRGO 触发；采集中按键锁定，松开再按才可重测。FFT 在 10～505 kHz 搜索谱峰，随后 USART1 输出基波、拟合 Vpp/RMS 与谐波参数摘要。AD9226/DCMI、USART3 串口屏、DDS、DAC 和其他 demo 均保留但当前不启动。

这是一个面向电子设计竞赛信号题的 STM32H750 模板工程，目标是把常用的信号产生、采集、频谱分析和串口调试能力提前搭好，后续按题目要求快速组合业务逻辑。

## 当前能力

- `SUPER_FFT`（按需启用）：ADC3 的 4096 点 FFT 测频模块。它使用 `PC2_C / ADC3_INP0`、DMA1 Stream3 和独立的 TIM6_TRGO；先以约 400 kS/s 粗测 10 kHz～100 kHz，低于 10 kHz 时改为约 40 kS/s 做 FFT 粗测与 1 Hz 步进细扫。当前默认主任务仍是 ADC2，`SUPER_FFT` 不会自动启动。

- AD9833 软件 SPI 驱动：支持寄存器、频率、相位和波形控制。
- AD9959 GPIO 软件 SPI 驱动：直接移植 `log.txt` 的寄存器操作与软件时序，支持参考代码中的四通道、调制和扫描接口；当前默认任务将四路配置为 100 kHz 正弦。
- AD9910 波形 API：支持幂等初始化、原始 14 位 ASF 单 Profile 正弦输出，以及按直接输出 mVpp 设置的正弦/三角波/方波；RAM 三角波和方波使用 50～1024 点自适应 polar 回放。十倍后级方案的满量程标定集中在 `HDL/AD9910_Constants.h`。
- ADS8688 GPIO 软件 SPI 驱动：支持命令/程序寄存、手动采样、量程换算和 VOFA+ FireWater 输出。
- AD9226 12 位并行采集：TIM1 输出 1 MHz 时钟，DCMI + DMA2 采集 4096 点，支持约 1 kHz 正弦的频率、基波至五次谐波和 THD 验证。
- DAC8830 驱动：支持 mV 码值接口和 `I_250730` 兼容的 V 单位直流接口。
- STM32H750 片上 DAC 波形输出：TIM4 触发 DAC1 CH1/CH2 的单缓冲循环 DMA，支持正弦、方波、三角波和直流；每种波形各有独立 256 点容量的表，高频时按 2 MS/s 上限缩短活跃点数，修改波形或参数前先停止再启动。
- ADC1 + ADC2 双重规则同步采样：频率测量帧以1 MS/s同时触发`PA1_C / ADC1_INP1`和`PA7 / ADC2_INP7`，相位帧根据测得频率自动调整TIM1分频和采样点数。
- CMSIS-DSP FFT：4096 点 FFT，支持汉宁窗、主峰/次峰、频率、幅值、Vpp、相位估计。
- 二阶 IIR 低通：基于 CMSIS-DSP DF1 双二阶实现，复刻`1 / (1e-8 s² + 3e-4 s + 1)`；保留 ADC→DAC demo，可按实际采样率重新初始化。
- 双正弦相位差：覆盖 1 kHz 到 100 kHz，FFT 粗定位后使用公共频率正弦拟合，并预留多频点相位校准表。
- 数字锁相鉴相器与 PI 环：`BLL/DLIA_BLL.*` 从 512 点同步双 ADC 码流提取相位差；`BLL/DPLL_BLL.*`将滤波相位转换为 PI 相位校正，并写入 AD9833 PHASE0。
- 方波频率测量：PA0 / TIM2_CH1 输入捕获，覆盖 1 Hz 到 1 MHz；低频使用捕获中断，高频使用 `/8` 捕获 DMA，输出分辨率分别为 0.5 Hz 和 10 Hz。
- 淘晶驰串口屏模板：USART3（PB10/PB11）115200 8N1，支持五任务选择、频率/Vpp键盘参数、文本/数值返回帧和结果页刷新；当前只维护 `RESERVED` 状态，不启动题目业务。页面和字节码见 [串口屏.md](串口屏.md)。

## 项目结构

- `Core/`：STM32CubeMX 生成的 HAL 初始化代码和 `main.c`。
- `HDL/`：硬件驱动层，放外部芯片和软件 SPI 等底层驱动。
- `FML/`：功能模块层，封装 ADC、DAC、FFT、测频、USART 等可复用功能。
- `BLL/`：业务逻辑层，放采样数据转换、FFT 结果整理、串口批量发送等逻辑。
- `API/`：应用入口层，放测频、采样帧处理和 FFT 打印等应用入口。
- `.agents/skills/adc-signal-capture-test/`：ADC采集信号测试技能，指导指定 ADC 引脚的循环 DMA 采集和 FireWater/VOFA+ 输出。
- `.agents/skills/stm32-adc-port-test/`：ADC端口预设技能，指导一次性 TIM 触发、DMA 采集、停止和 VOFA+ 发送流程。
- `.agents/skills/stm32h750-iir-ad-da-filter/`：ADC1→二阶 IIR→DAC1 实时低通技能，涵盖 DMA、缓存一致性、对照测试与噪声排查。
- `Drivers/`、`Middlewares/`：STM32 HAL、CMSIS 和 DSP 相关依赖。
- `cmake/`、`CMakeLists.txt`：CMake/Ninja 构建配置。
- `Tools/`：统一的 PowerShell 编译/烧录入口；本机路径位于被 Git 忽略的 `Tools/config/local.ps1`，示例见 `local.ps1.example`。
- [ADS8688_PORTING.md](ADS8688_PORTING.md)：ADS8688 软件 SPI、CH1~CH4 手动通道轮询、FireWater 和 USART DMA 的可移植说明。

## 默认启动流程

`Core/Src/main.c` 当前初始化 GPIO、DMA 和 USART3。启动阶段配置 AD9833 为 1 kHz、0 deg 正弦，配置 AD9910 为 100 kHz、300 mVpp 模块直接输出正弦；两路配置完成后由 DDS 芯片独立持续输出。串口屏通过 USART3 `PB10/PB11`、115200 8N1 接入，主循环只运行 `TJC_HMI_API_Process()`。AD9959、TIM2、ADC、DAC、USART1、SPI 外设与其他任务均不启动。

串口屏完成上电后会发送 ready 帧，固件将其切回 `home` 页面；页面与字节码配置见 [串口屏.md](串口屏.md)。AD9910 的 300 mVpp 为模块直接输出端目标值，实际频率和 Vpp 仍需用示波器验收。

如需在 `while` 前启动 DDS，可调用 `AD9833_API_OutputWaveform(frequency_hz, waveform)` 或 `AD9910_API_OutputSine(frequency_hz, amplitude_mvpp)`；两者都是 `void` 封装，内部完成各自所需的初始化和输出配置。

## 构建

需要命令行构建时，先复制 `Tools/config/local.ps1.example` 为 `Tools/config/local.ps1` 并填写本机 CubeIDE VS Code / OpenOCD 路径，再使用统一入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1
```

构建目录或缓存失效时，在命令末尾追加 `-Reconfigure`；明确需要烧录时将脚本改为 `Tools\flash.ps1`，该脚本总会先成功构建再烧录。未明确要求时，自动化流程不应自行编译或改写板上 FLASH。

## 关键配置

- MCU：STM32H750XBHx，工程名 `adc_fft_demo`。
- USART1：PB6 TX、PB7 RX，921600 8N1；当前主任务不初始化。PB6/PB7 与 AD9226 资源冲突，不能同时启动。
- USART3：PB10 TX、PB11 RX，115200 8N1；当前主任务驱动淘晶驰串口屏页面状态机。
- AD9226：D0～D11 使用 PC6/PC7/PC8/PC9/PE4/PB6/PE5/PE6/PC10/PC12/PB5/PD2，PA8 输出 1 MHz CLK 并回接 PA6/PIXCLK，PB1→PA4、PB2→PB7 为 DCMI 同步门控回路。
- 方波测频：PA0 / TIM2_CH1，上升沿输入捕获；TIM2 时钟当前为 75 MHz，DMA 使用 DMA1 Stream5。
- ADC1：PA1_C / ADC1_INP1；当前主任务不初始化，保留给后续 ADC 端口预设或其他任务。
- ADC2：PA7 / ADC2_INP7；当前独立采集链路由 TIM1_TRGO 触发，目标/实际均为 1 MS/s，10 bit、1024 点 halfword normal DMA（DMA1 Stream1）。
- ADC3：PC2_C / ADC3_INP0；保留给 `ADC_VOFA_API_*` 的端口预设，当前主任务不初始化。
- 当前 CubeMX 生成时钟：CPU 480 MHz、AXI/AHB 240 MHz；其他会变的时钟和资源事实见 `STATUS.md`。
- 片上 DAC1：PA4 / DAC1_OUT1；当前主任务使用 TIM4_TRGO、DMA1 Stream6 和 20 点循环 DMA 输出 100 kHz、1 Vpp、1.65 V 偏置正弦，更新率为 2 MS/s。IIR demo 仍保留其独立的 TIM4_TRGO、1 MS/s、1024 点半缓冲调度。
- ADC 采样：IIR demo 使用 12 bit、TIM1 TRGO、循环 DMA；当前端口预设使用 ADC1 10 bit、TIM1 TRGO 和 normal DMA。切换回双 ADC 测频/相位或 IIR 任务时须恢复对应 `.ioc` 配置。
- AD9833：`FSYNC/CS=PA1`、`SDATA=PH4`、`SCLK=PH5`。本板封装中 PA1 与 PA1_C 是独立焊盘；ADC1 使用 PA1_C，ADC 直连开关保持打开即可与 PA1 的 AD9833 片选并存。
- AD9910：见 `Core/Inc/main.h` 中 `MRT/PF0/PF1/PF2/IUP/CSN/SDI/SCK9` 宏。
- AD9959：当前 demo 按 `log.txt` 使用 GPIO 软件 SPI，`SCLK=PB3`、`SDIO0=PD7`、`CS=PC7`、`IO_UPDATE=PC0`、`RESET=PE4`、`PDC=PC1`。PDC 必须接线并保持低电平；PB3/PD7 与 DAC8830 硬件 SPI 冲突，PC7/PE4 与 AD9226 冲突。ADS8688 的 `PB13/PB14/PB15` 未被占用。假设系统时钟为 500 MHz；AD9910 的 `PA6/PA8/PD4/PD5/PA12` 控制线不被使用。`SDIO1`～`SDIO3`、`PS0`～`PS3` 必须按模块要求固定电平，不能悬空。模块若以 5 V 逻辑供电，先核验高电平门限并配置电平转换。
- DAC8830：默认 `CS1=PE2`、`CS2=PE0`、`SDI=PD7/SPI1_MOSI`、`SCLK=PB3/SPI1_SCK`；当前 DMA demo 使用 TIM4 产生 1.2 MS/s 更新节拍，不占用 ADC2 的 `PA7` 或 DAC1_CH2 的 `PA5`。
- ADS8688：`CS=PB12`、`RST_PD=PC4`、`SCK=PB13`、`SDO=PB14`、`SDI=PB15`；当前均由 GPIO 软件 SPI 驱动。当前使用 CH1～CH4 手动通道轮询与 `+-10.24 V` 量程。

更多接线和开发注意事项见 [GUIDE.md](GUIDE.md)，当前状态见 [STATUS.md](STATUS.md)。

## DAC8830 Default Task

The current `main.c` initializes GPIO, DMA, TIM4, and SPI1, then starts the
DAC8830 DMA sine demonstration. Both DAC8830 chip selects are asserted by the
DMA path, so both connected DAC8830 outputs receive the same 1 kHz, 1 Vpp
sine. The module must be configured for the unipolar `+5 V` range. Per the
module manual, the driver uses a 0-5.000 V transfer range, so the sine is
centered at 2.500 V and spans approximately 2.000 V to 3.000 V.

## ADC2 FFT Measurement And Waveform Classification

The current `main.c` task keeps the AD9833 and AD9910 DDS outputs active, then
captures ADC2 (`PA7 / ADC2_INP7`) with TIM1 TRGO and DMA1 Stream1. It captures a
4096-sample coarse frame at approximately 409.6 kS/s, searches the 1-100 kHz
range and available 360-409.6 kS/s TIM1 dividers for lower cycle-closure error,
and captures one final frame. The final result reports measured frequency plus
`wave=sine|triangle|square|unknown` and the `h3`/`h5` harmonic ratios. TIM2
frequency measurement initialization and processing calls remain in `main.c` as
comments and are not active.
## Historical task: AD9226 5 MS/s VOFA+ capture test

At boot, the AD9226 test uses the reference DCMI wiring unchanged: TIM1_CH1 on `PA8` drives the ADC clock and is physically looped to `PA6 / DCMI_PIXCLK`; `PB1 -> PA4 / HSYNC` and `PB2 -> PB7 / VSYNC` provide the software frame gate. It captures one 8192-sample 12-bit frame at 5 MS/s through DCMI/DMA2, then stops acquisition.

USART3 `PB10/PB11` (board P10/P11) at 115200 8N1 sends the completed raw frame to VOFA+ as FireWater `ad9226_raw_mV:<value>` frames. Voltage conversion uses the reference board's 10 V span and zero-code calibration only for waveform observation; it must be recalibrated before any amplitude claim. ADC1/G1 FFT and the older AD9226 THD API remain in the source tree but are not started by `main.c`.

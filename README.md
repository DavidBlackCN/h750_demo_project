# STM32H750 电赛信号题模板项目

当前主任务同时启动 AD9833、AD9910 与片内 DAC1：AD9833 输出 1 kHz 正弦，AD9910 输出 100 kHz 正弦，DAC1_CH1 / PA4 经 TIM4 触发 DMA 输出 1 kHz 方波；USART1 可运行时修改片内 DAC 参数。

这是一个面向电子设计竞赛信号题的 STM32H750 模板工程，目标是把常用的信号产生、采集、频谱分析和串口调试能力提前搭好，后续按题目要求快速组合业务逻辑。

## 当前能力

- `SUPER_FFT`（按需启用）：ADC3 的 4096 点 FFT 测频模块。它使用 `PC2_C / ADC3_INP0`、DMA1 Stream3 和独立的 TIM6_TRGO；先以约 400 kS/s 粗测 10 kHz～100 kHz，低于 10 kHz 时改为约 40 kS/s 做 FFT 粗测与 1 Hz 步进细扫。当前默认主任务仍是 ADC2，`SUPER_FFT` 不会自动启动。

- AD9833 软件 SPI 驱动：支持寄存器、频率、相位和波形控制。
- AD9910 波形 API：支持幂等初始化、原始 14 位 ASF 单 Profile 正弦输出，以及按直接输出 mVpp 设置的正弦/三角波/方波；RAM 三角波和方波使用 50～1024 点自适应 polar 回放。十倍后级方案的满量程标定集中在 `HDL/AD9910_Constants.h`。
- ADS8688 硬件 SPI 驱动：支持命令/程序寄存、单通道手动采样、量程换算和 VOFA+ FireWater 输出。
- AD9226 12 位并行采集：TIM1 输出 1 MHz 时钟，DCMI + DMA2 采集 4096 点，支持约 1 kHz 正弦的频率、基波至五次谐波和 THD 验证。
- DAC8830 驱动：已移植高层电压/码值接口，支持 TIM4 + DMA 驱动 SPI1 输出波形。
- STM32H750 片上 DAC 波形输出：TIM4 触发 DAC1 CH1/CH2 的单缓冲循环 DMA，支持正弦、方波、三角波和直流；每种波形各有独立 256 点表，修改波形或参数前先停止再启动。
- ADC1 + ADC2 双重规则同步采样：频率测量帧以1 MS/s同时触发`PA1_C / ADC1_INP1`和`PA7 / ADC2_INP7`，相位帧根据测得频率自动调整TIM1分频和采样点数。
- CMSIS-DSP FFT：4096 点 FFT，支持汉宁窗、主峰/次峰、频率、幅值、Vpp、相位估计。
- 二阶 IIR 低通：基于 CMSIS-DSP DF1 双二阶实现，复刻`1 / (1e-8 s² + 3e-4 s + 1)`；保留 ADC→DAC demo，可按实际采样率重新初始化。
- 双正弦相位差：覆盖 1 kHz 到 100 kHz，FFT 粗定位后使用公共频率正弦拟合，并预留多频点相位校准表。
- 数字锁相鉴相器与 PI 环：`BLL/DLIA_BLL.*` 从 512 点同步双 ADC 码流提取相位差；`BLL/DPLL_BLL.*`将滤波相位转换为 PI 相位校正，并写入 AD9833 PHASE0。
- 方波频率测量：PA0 / TIM2_CH1 输入捕获，覆盖 1 Hz 到 1 MHz；低频使用捕获中断，高频使用 `/8` 捕获 DMA，输出分辨率分别为 0.5 Hz 和 10 Hz。
- 淘晶驰串口屏：USART3（PB10/PB11）115200 8N1，支持文本/数值刷新和触摸命令回传；USART1 保留为 USB 调试串口。

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

## 默认启动流程

`Core/Src/main.c` 当前不启动 ADC 采集任务。上电后先初始化并启动 AD9833 的 1 kHz 正弦与 AD9910 的 100 kHz、500 mVpp 正弦，再启动片内 `DAC1_CH1 / PA4` 的 1 kHz、1 Vpp、1.65 V 偏置方波。DAC 使用 TIM4_TRGO 和 DMA1 Stream6 的单缓冲循环 DMA；USART1 使用 PB6/PB7、921600 8N1 接收 DAC 参数命令。ADC、AD9226/DCMI、DAC8830、SPI 外设和 USART3 不启动。AD9910 初始化将 PA8 配置为软件 SCK，因此不能同时将 PA8/TIM1_CH1 用作外部时钟输出。

USART1 命令以回车结束，格式为 `波形编号 频率Hz Vpp 偏置V`。波形编号：`0` 正弦、`1` 方波、`2` 三角波、`3` 直流。例如发送 `2 1000 1.0 1.65` 会输出 1 kHz、1 Vpp、1.65 V 偏置三角波；固件回复 `ok` 或 `err`。更新会短暂停止并重启片内 DAC DMA，AD9833 和 AD9910 不受影响。

如需在 `while` 前启动 DDS，可调用 `AD9833_API_OutputWaveform(frequency_hz, waveform)` 或 `AD9910_API_OutputSine(frequency_hz, amplitude_mvpp)`；两者都是 `void` 封装，内部完成各自所需的初始化和输出配置。

## 构建

需要命令行构建时，先复制 `Tools/config/local.ps1.example` 为 `Tools/config/local.ps1` 并填写本机 CubeIDE VS Code / OpenOCD 路径，再使用统一入口：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1
```

构建目录或缓存失效时，在命令末尾追加 `-Reconfigure`；明确需要烧录时将脚本改为 `Tools\flash.ps1`，该脚本总会先成功构建再烧录。未明确要求时，自动化流程不应自行编译或改写板上 FLASH。

## 关键配置

- MCU：STM32H750XBHx，工程名 `adc_fft_demo`。
- USART1：PB6 TX、PB7 RX，921600 8N1；当前主任务用于片内 DAC 参数命令。PB6/PB7 与 AD9226 资源冲突，不能同时启动。
- USART3：PB10 TX、PB11 RX，115200 baud，当前用于 AD9226 摘要输出。
- AD9226：D0～D11 使用 PC6/PC7/PC8/PC9/PE4/PB6/PE5/PE6/PC10/PC12/PB5/PD2，PA8 输出 1 MHz CLK 并回接 PA6/PIXCLK，PB1→PA4、PB2→PB7 为 DCMI 同步门控回路。
- 方波测频：PA0 / TIM2_CH1，上升沿输入捕获；TIM2 时钟当前为 75 MHz，DMA 使用 DMA1 Stream5。
- ADC1：PA1_C / ADC1_INP1；当前主任务不初始化，保留给后续 ADC 端口预设或其他任务。
- ADC2：PA7 / ADC2_INP7；当前独立采集链路由 TIM1_TRGO 触发，目标/实际均为 1 MS/s，10 bit、1024 点 halfword normal DMA（DMA1 Stream1）。
- ADC3：PC2_C / ADC3_INP0；保留给 `ADC_VOFA_API_*` 的端口预设，当前主任务不初始化。
- 当前 CubeMX 生成时钟：CPU 480 MHz、AXI/AHB 240 MHz；其他会变的时钟和资源事实见 `STATUS.md`。
- 片上 DAC1：PA4 / DAC1_OUT1；当前主任务使用 TIM4_TRGO、DMA1 Stream6 和 256 点单缓冲循环 DMA 输出 1 kHz 正弦。IIR demo 仍保留其独立的 TIM4_TRGO、1 MS/s、1024 点半缓冲调度。
- ADC 采样：IIR demo 使用 12 bit、TIM1 TRGO、循环 DMA；当前端口预设使用 ADC1 10 bit、TIM1 TRGO 和 normal DMA。切换回双 ADC 测频/相位或 IIR 任务时须恢复对应 `.ioc` 配置。
- AD9833：`FSYNC/CS=PA1`、`SDATA=PH4`、`SCLK=PH5`。本板封装中 PA1 与 PA1_C 是独立焊盘；ADC1 使用 PA1_C，ADC 直连开关保持打开即可与 PA1 的 AD9833 片选并存。
- AD9910：见 `Core/Inc/main.h` 中 `MRT/PF0/PF1/PF2/IUP/CSN/SDI/SCK9` 宏。
- DAC8830：默认 `CS1=PE2`、`CS2=PE0`、`SDI=PA7/SPI1_MOSI`、`SCLK=PA5/SPI1_SCK`；当前 DMA demo 使用 TIM4 产生 2.5 MS/s 更新节拍。
- ADS8688：`CS=PB12`、`RST_PD=PC4`、`SCK=PB13/SPI2_SCK`、`SDO=PB14/SPI2_MISO`、`SDI=PB15/SPI2_MOSI`；SPI Mode 1，SCK 17 MHz。

更多接线和开发注意事项见 [GUIDE.md](GUIDE.md)，当前状态见 [STATUS.md](STATUS.md)。

## ADC2 FFT Task

The current `main.c` task keeps the AD9833 and AD9910 DDS outputs active, then
captures ADC2 (`PA7 / ADC2_INP7`) with TIM1 TRGO and DMA1 Stream1. It captures a
4096-sample coarse frame at approximately 409.6 kS/s, searches the available
360-409.6 kS/s TIM1 dividers for lower cycle-closure error, and captures one
final 4096-sample frame.
The actual sample rate is derived from the active TIM1 clock and is reported on
UART1 (`PB6/PB7`, 921600 8N1).

`adc2_proc()` only drives capture completion and raw-frame publication. FFT code
is independent in `FML/ADC2_FFT_FML.*` and `BLL/ADC2_FFT_BLL.*`, where analysis
accepts only a raw array, sample count, and sample rate. The UART output contains
the final frame: a `raw begin` line, 4096 `raw:<code>` lines, `raw end`, a
2048-bin `spectrum begin` / `fft:<bin>,<amplitude-v>` / `spectrum end` block,
and one `result` line containing the measured peak frequency and frequency
resolution.
Disconnect any AD9226 wiring from PB6/PB7 while using USART1.

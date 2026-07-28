# 当前主任务：AD9959 四通道 100 kHz 正弦 + TIM2 测频

`main.c` 当前初始化 AD9959 和 TIM2 输入捕获。AD9959 的 CH0～CH3 均为 100 kHz、幅度码 512、相位码 0；控制线为 `PA8=SCLK`、`PD4=CS`、`PD5=IO_UPDATE`、`PA12=SDIO0`、`PA6=RESET`。TIM2 通过 `PA0 / TIM2_CH1` 测量 0～3.3 V 外部方波，并在 USART1 `PB6/PB7`（921600 8N1）每秒输出一次频率摘要。AD9959 假设模块系统时钟为 500 MHz，必须按实际模块改写；5 V 数字逻辑模块应确认高电平门限或增加电平转换，且模块需按实际功耗散热。

# 保留任务：DDS 与片内 DAC 同时输出

`main.c` 上电后启动 AD9833 的 1 kHz 正弦、AD9910 的 100 kHz、500 mVpp 正弦，以及 `DAC1_CH1 / PA4` 的 1 kHz、1 Vpp、1.65 V 偏置方波。片内 DAC 使用 TIM4_TRGO 和 DMA1 Stream6 的 256 点单缓冲循环 DMA。USART1 的 PB6/PB7 以 921600 8N1 接收片内 DAC 参数命令。当前不启动 ADC、TIM1、AD9226/DCMI、DAC8830、SPI 外设或 USART3。AD9910 会占用 `PA8` 作为软件 SCK，因此不能同时把 `PA8/TIM1_CH1` 用作外部时钟输出。

串口命令用一行四个数字、以回车结束：`波形编号 频率Hz Vpp 偏置V`。编号为 `0=正弦`、`1=方波`、`2=三角波`、`3=直流`。例如：

```text
2 1000 1.0 1.65
```

会将 PA4 改为 1 kHz、1 Vpp、1.65 V 偏置三角波。参数范围为频率大于 0，`Vpp` 和偏置均为 0 到 3.3 V；成功返回 `ok`，格式或参数错误返回 `err`。每次更新会先停止再启动片内 DAC DMA，因此 PA4 有极短间断；AD9833 和 AD9910 的输出持续运行。

## ADC3 SUPER_FFT 测频（按需启用）

`API/SUPER_FFT.*` 是独立的 ADC3 测频任务，不改变当前 ADC2 默认主任务。启用时在 `main.c` 的初始化阶段调用 `MX_ADC3_Init()`，再调用 `SUPER_FFT_Start()`；主循环持续调用 `SUPER_FFT_Process()`，直至 `SUPER_FFT_IsReady()` 为真，然后用 `SUPER_FFT_GetFrequencyHz()` 读取结果。不要与 `ADC_VOFA_API_*` 同时使用 ADC3。

ADC3 输入为 `PC2_C / ADC3_INP0`，电压必须限制在 0～3.3 V。DMA1 Stream3 的 4096 点 halfword 缓冲区在 `.dma_buffer` / RAM_D2、32 字节对齐；每帧开始前清理缓存、完成后在前台失效缓存。TIM6 不占用任何外部引脚，运行时根据实际 APB1 定时器时钟计算 PSC/ARR，输出 UPDATE TRGO；当前时钟配置下目标/实际采样率为 400 kS/s 和 40 kS/s。ADC3 为 12 bit、8.5 周期采样时间，ADC 时钟约 80 MHz，远高于最高 400 kS/s 的转换需求。

如需上电即输出 DDS 波形，可在 `/* USER CODE BEGIN 2 */`、`while` 前直接调用：

```c
AD9833_API_OutputWaveform(1000.0f, AD9833_OUT_SINUS);
AD9910_API_OutputSine(100000U, 500U);
```

前者的第二个参数可选 `AD9833_OUT_SINUS`、`AD9833_OUT_TRIANGLE`、`AD9833_OUT_MSB` 或 `AD9833_OUT_MSB2`；后者的幅度单位与既有 `AD9910_output_sine()` 完全一致，表示固定十倍后级的目标 mVpp。

| 连接对象 | STM32H750 MCU 引脚 | 配置 |
| --- | --- | --- |
| ADC2 待测模拟信号 | `PA7 / ADC2_INP7` | 单端 0～3.3 V；与迁移后的 DAC8830 SPI1 引脚无复用 |
| USB-TTL 接收端 RX | `PB6 / USART1_TX` | 固件输出，921600 baud、8N1、无流控 |
| USB-TTL 发送端 TX（可不接） | `PB7 / USART1_RX` | 本预设不接收命令；若接入，仍须与 TX 交叉 |
| USB-TTL 地 / 信号源地 | `GND` | 必须共地 |

目标采样率为 1 MS/s。TIM1 计数时钟为 240 MHz，使用 PSC=0、ARR=239，实际采样率为 1,000,000 S/s；ADC2 使用 80 MHz 时钟、10 bit、8.5 周期采样时间。一个采样周期为 80 个 ADC 时钟，满足 10 bit 转换时间；DMA 缓冲区位于 `.dma_buffer` / RAM_D2、32 字节对齐，并在 DMA 前后维护 D-Cache。

USART1 使用 921600、8N1、无流控。`adc2_proc()` 复用参考工程的 `Usart_Send_ADC_Data()`，输出格式为每个换算后的电压一行：`%.5f\r\n`，共 1024 行；使用 VOFA+ 时选择可逐行接收数值的文本协议，不混入其他日志。新开发板的丝印/排针位置必须以其原理图或 pinout 对应上述 MCU 管脚为准，不能把不同 H750 板的排针编号当作通用定义。

验收时先断电接线，确认 `PA7` 电压未超过 0～3.3 V，再上电观察单帧曲线。成功编译、烧录、串口收帧和示波器确认输入波形分别是不同验证结论；本次尚未执行后三项。

# 保留说明：AD9833 数字 PI 锁相 demo

当前主程序以 TIM1_TRGO 同步采集 ADC1/ADC2 并运行数字鉴相器。AD9833 软件 SPI 引脚为`PA1=FSYNC`、`PH4=SDATA`、`PH5=SCLK`；本板 MCLK 为 25 MHz，启动时输出 FREQ0=10 kHz、PHASE0=0°正弦。前台每 10 ms将内部`ADC2-ADC1`相位转换为示波器标准`CH2-CH1`后更新 AD9833 PHASE0；ADC、AD9833 与 USART1 会启动，DAC 不启动。

PI 接线：信号发生器输出经跟随、1.65 V 偏置和 0~3.3 V 限幅后接`PA1_C / ADC1_INP1`，并接示波器 CH2；AD9833 输出经同样调理后接`PA7 / ADC2_INP7`，并接示波器 CH1。两路和开发板必须共地。内部原始测量是`ADC2-ADC1=CH1-CH2`；PI 已显式取反，故`DPLL_API_TARGET_PHASE_DEG`、校准值和验收统一采用示波器标准`CH2-CH1`。若交换两路 ADC 或示波器通道，必须重新确认符号和控制方向。

PI 初值集中在`API/DPLL_API.h`：目标相位`DPLL_API_TARGET_PHASE_DEG=0`、校准`DPLL_API_PHASE_CALIBRATION_DEG=0`、初始命令`DPLL_API_INITIAL_PHASE_COMMAND_DEG=180`、`Kp=0.10`、`Ki=0.050`、更新周期10 ms。初始180°用于避开本接线下的反相捕获点；积分相位按360°环绕，不能改回硬钳位，否则两台独立时钟的微小频差会使相位指令在边界冻结。先在示波器确认相位方向和固定偏差，再填入校准值；开始调参时先将`Ki`设为0，仅调小`Kp`确认不振荡，然后逐步增加`Ki`。串口`phase`是闭环使用的滤波相位，`raw`只用于观察瞬时噪声。

运行中可通过 USART1（PB6=TX、PB7=RX，921600 8N1，以回车结束）直接调参。串口适配器必须交叉连接：适配器 TX→PB7、适配器 RX←PB6，并与开发板共地。发送`kp 0.10`或`ki 0.01`；`show`打印当前参数、测得相位、误差和相位命令。每次`kp`、`ki`或`show`命令只回传一次`dpll ...`摘要，不在闭环运行中周期输出；原`phase=...`鉴相诊断在当前 PI 主任务默认关闭。命令只影响 RAM 中的 Kp/Ki，复位后恢复`DPLL_API.h`的默认值。

# 开发指南

## 分层约定

新功能优先按现有四层组织：

- `HDL`：只处理芯片寄存器、GPIO、SPI/I2C/并口时序等硬件细节。
- `FML`：把一个硬件能力封装成稳定功能，例如波形输出、ADC 采样、FFT 计算。
- `BLL`：做数据换算、结果整理、业务判断，不直接散落硬件时序。
- `API`：放应用级入口，供 `main.c` 主循环或初始化流程调用。

简单外设驱动可以先只放 `HDL`。等需要演示流程或题目业务时，再补 `API/BLL/FML`。

## 添加新驱动

1. 先读同类驱动：外部芯片看 `HDL/AD9833.*`、`HDL/AD9910.*`、`HDL/DAC8830.*`。
2. 新增 `.h/.c` 到合适目录。
3. 在根目录 `CMakeLists.txt` 的 `target_sources()` 里加入新的 `.c`。
4. 引脚宏尽量集中在头文件或 `Core/Inc/main.h`，方便比赛现场换线。
5. 不要无理由改 CubeMX 生成区。必须改时优先放在 `USER CODE` 区，或在独立模块里覆盖配置。
6. 用户明确要求编译验证时执行 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1`；新建构建目录或缓存失效时追加 `-Reconfigure`。

## AD9959 四通道 DDS（按需接入）

`HDL/AD9959.*` 参考 [AD9959 驱动文章](https://2048ai.net/682ae80d606a8318e8576db5.html) 的寄存器流程移植到 STM32H750 HAL：GPIO 软件串行写寄存器、CSR 通道选择、CFTW0 频率控制字、ACR 幅度控制字、CPOW0 相位控制字，最后以 `IO_UPDATE` 锁存。

驱动没有固定开发板引脚，也不由当前 `main.c` 启动。先选择五根未占用的 3.3 V GPIO，并在应用初始化阶段传入配置；`system_clock_hz` 必须填模块实际 DDS 系统时钟。文章的参考模块使用 25 MHz 晶振和 20 倍 PLL，即 500 MHz。频率控制字按实际时钟用 64 位整数计算，不依赖文章中的固定 `8.589934592` 系数。

```c
#include "AD9959.h"

const AD9959_Config ad9959 = {
    .sclk_port = GPIOx, .sclk_pin = GPIO_PIN_x,
    .cs_port = GPIOx, .cs_pin = GPIO_PIN_x,
    .update_port = GPIOx, .update_pin = GPIO_PIN_x,
    .sdio0_port = GPIOx, .sdio0_pin = GPIO_PIN_x,
    .reset_port = GPIOx, .reset_pin = GPIO_PIN_x,
    .system_clock_hz = 500000000U,
    .sclk_half_period_nops = 8U,
};

if (AD9959_Init(&ad9959) == HAL_OK) {
    (void)AD9959_ConfigureSingleTone(AD9959_CHANNEL_0,
                                     1000000U, 512U, 0U);
}
```

`AD9959_CHANNEL_0` 到 `AD9959_CHANNEL_3` 可按位或组合选择。组合选择时，所有被选通道写入相同的频率、幅度和相位；如需各通道相位不同，则依次调用每个通道。幅度码范围为 `0`～`1023`，相位码范围为 `0`～`16383` 对应 `0`～`360°`。`AD9959_ConfigureSingleTone()` 在三个寄存器都写完后只产生一次 `IO_UPDATE` 脉冲。

当前 `main.c` 的 AD9959 + 测频 demo 使用 `PA8=SCLK`、`PD4=CS`、`PD5=IO_UPDATE`、`PA12=SDIO0`、`PA6=RESET`，将 CH0～CH3 同时配置为 `100 kHz`、幅度码 `512`、相位码 `0`。这五根线复用了未启动 AD9910 的控制 GPIO，故不能同时启动 AD9910；`PA6/PA8` 也不能同时用作 AD9226 的时钟回路。TIM2 测频输入固定为 `PA0 / TIM2_CH1`，输入须为共地的 0～3.3 V 方波。USART1 `PB6/PB7` 以 921600 8N1 每秒输出一行 `freq=...` 摘要。

模块数字侧通常为 5 V 供电，不能将其 `SDIO1`～`SDIO3` 或其他 5 V 信号直接接入 H750。当前驱动只使用 MCU 到模块的 `SDIO0` 单向写入；首次接线仍应确认 3.3 V 输出能满足模块高电平门限，必要时加入电平转换。AD9959 功耗和发热较高，须按模块实际供电与散热条件上板验收。

## DAC8830 使用示例

```c
#include "DAC8830.h"

DAC8830_Init();
DAC8830_SelectBipolar10V();
DAC8830_SetVoltageMvA(1000);   // OUTA = +1.000 V
DAC8830_SetVoltageMvB(-1000);  // OUTB = -1.000 V
DAC8830_WriteBoth(32768U);     // 两路直接写 16 bit 码值
```

为兼容 `D:\I_250730` 的 DAC8830 驱动，也保留了以下以 V 为单位的接口：

```c
DAC8830_SelectUnipolar5V();
DAC8830_Set_Direct_Current(2.5);        // 两路输出 2.500 V
DAC8830_Set_Mode_Voltage(OUTPUT_DC, 1.0);
```

`DAC8830_Set_Wave()` 和 `DAC8830_Generate_Wave_Data()` 会在前台逐点写 SPI，
仅用于兼容和低速调试。

默认输出模式是 `DAC8830_OUTPUT_BIPOLAR_10V`。如果模块跳线不是 ±10 V，请调用对应选择函数，或用 `DAC8830_SetRangeMv(min, max)` 设置自定义量程。

DAC8830 的高频正弦 TIM4/DMA 后端已移除。当前主任务仅使用 SPI1 将直流码值写入 DAC8830；若后续重新实现波形输出，需独立验证 SPI 带宽、片选时序、DMA 触发与模拟重构滤波。

## 片上 DAC 波形输出

`FML/DAC_FML.*` 提供：

- `DAC_Waveform_Start(type, frequency_hz, vpp, offset_v)`
- `DAC_Waveform_Apply(type, frequency_hz, vpp, offset_v)`
- `DAC_Waveform_StartChannel(channel, type, frequency_hz, vpp, offset_v)`
- `DAC_Waveform_Stop()`

波形缓冲区容量为 256 点，输出频率由 `TIM4` 更新触发控制，输出电压限制在 0 到 3.3 V。正弦、方波、三角波和直流各自拥有一张独立 DMA 源表，使用普通循环 DMA 输出；高频时 API 按 2 MS/s 上限缩短活跃点数。运行中的 `Start`、`Apply` 或 `StartChannel` 返回 `HAL_BUSY`，避免改写 DMA 正在读取的表；应先调用 `DAC_Waveform_Stop()`，再以新参数启动。DMA 或 DAC 欠载错误后接口返回 `HAL_ERROR`，同样需停止后重新启动。该机制与 IIR ADC→DAC 链路使用的同侧半缓冲回写相互独立。

## 二阶 IIR 低通

`FML/IIR_FML.*` 使用与参考工程相同的 CMSIS-DSP `arm_biquad_cascade_df1_f32` 路径，复刻下列单位直流增益传递函数：

```text
                    1
H(s) = --------------------------------
       1e-8 s^2 + 3e-4 s + 1
```

调用 `IIR_Lowpass_Init(&filter, actual_sample_rate_hz)` 时按双线性变换生成系数。参考工程的采样率是 2 MS/s，此时 CMSIS 系数为`[6.203435e-6, 1.240687e-5, 6.203435e-6, 1.9850869, -0.9851118]`，顺序为`b0, b1, b2, a1, a2`。滤波状态在每次`IIR_Lowpass_Process()`后保留，连续 DMA 半块必须复用同一 `iir_lowpass_filter_t`，不要逐块重新初始化。

对于 12 位 offset-binary ADC 到片上 DAC 的缓冲区处理，可使用：

```c
static iir_lowpass_filter_t filter;
static q15_t filter_q15_scratch[DMA_HALF_SAMPLES];
static float32_t filter_float_scratch[DMA_HALF_SAMPLES];

IIR_Lowpass_Init(&filter, IIR_LOWPASS_REFERENCE_SAMPLE_RATE_HZ);
IIR_Lowpass_ProcessAdc12ToDac12(&filter,
                                adc_half, dac_half, filter_q15_scratch,
                                filter_float_scratch,
                                DMA_HALF_SAMPLES, 2048U, 2048U);
```

上述 helper 会以 Q15 格式移除 ADC 中点，转换至 float32 完成滤波，再量化为 Q15、恢复 DAC 中点并限制输出到 0～4095。保留的 IIR demo 使用`FML/IIR_ADDA_FML.*`把该 helper 接入 ADC1→DAC1：1024 点循环 DMA 分为两个 512 点半块，ADC 完成一半时只写 DAC 的另一半，避免改写 DMA 正在发送的数据。TIM1/TIM4 均为 1 MS/s，滤波状态跨半块保持；输出因此具有一个半块（512 µs）延迟。

上板验证时将调理后的 0～3.3 V 信号接入`PA1_C`，从`PA4 / DAC1_OUT1`观察输出；先用 100 Hz、1 kHz、10 kHz、小于 3 Vpp 且中心为 1.65 V 的正弦，测量输入和输出 Vpp 比，对照本节列出的理论增益。不要在本任务中初始化 AD9226/DCMI、双 ADC 相位采集或 DAC8830；它们与 ADC1、TIM1、PA4、DMA1 Stream0/1 存在资源冲突。

## ADC + FFT 流程

当前采样处理链路：

1. CPU运行在480 MHz，TIM1计数时钟为240 MHz。
2. 频率阶段以1 MS/s同步采集4096组ADC1/ADC2数据，4096点FFT粗定位后通过正弦拟合得到公共频率。
3. 相干阶段根据公共频率联合搜索TIM1整数分频、3072～4096点采样长度和整数周期数，采样率保持在1～1.1 MS/s并优先接近1 MS/s，同时使窗口首尾相位闭合误差最小。
4. 用搜索得到的实际采样率和第一阶段公共频率拟合两路相位，计算`ADC2相位 - ADC1相位 - 校准值`。
5. 完成相位拟合后只输出频率和相位差，然后重新回到1 MS/s测频阶段。

相位帧会同时检查两路三次谐波与基波幅度比。两路比值都不低于0.04时，按三角波/强谐波波形处理：先在一个周期内搜索两路归一化互相关峰，再用黄金分割细化到小数采样点延迟，最后把时延换算为相位。未检测到强三次谐波时仍使用原正弦最小二乘相位，因此正弦测量路径不变。三角波互相关低于0.98的帧不输出。

当前DMA采用normal模式，频率帧和相位帧使用同一缓冲区但分时采集。4096点FFT只负责先测频，相位差只使用后续相干采样帧。

## 数字锁相鉴相器（算法自检）

`BLL/DLIA_BLL.*`移植了参考工程的正交 IQ 解调：1024 点正弦查表加线性插值生成 32 位 DDS 参考；每帧固定处理 512 个同步 ADC 码值，并用块内均值消除直流偏置。两路在同一循环中使用同一个相位累加器，避免顺序调用时把第二路参考相位推进一个数据块。相位直接使用当前帧 I/Q；幅值只对模长做一阶低通，避免 normal DMA 帧间暂停造成 I/Q 向量抵消。输出为各通道的 I/Q、电压峰值、相位，以及 `ADC2相位 - ADC1相位` 并包络至`(-180°, 180°]`。

`Tests/dlia_bll_selftest.c`是主机离线自检，生成 15.625 kHz、1 MS/s、不同直流偏置和幅相的两路 12 bit 合成 ADC 数据；通过条件为相位差误差不超过 0.15°、通道 1 峰值幅度误差不超过 3 mV。该结果只验证 C 鉴相算法与量化误差，不含 ADC 同步、前端失配、D-Cache、AD9833 时钟或实物噪声。

后续接入 AD9833 锁相环时，应由双 ADC normal DMA 在帧完成后调用`DLIA_BLL_ProcessPair()`；P 项写 AD9833 相位字，I 项写频率字。不要在 DMA 回调中位操作软件 SPI；先完成 D-Cache 失效和鉴相，再由前台任务限速写寄存器。

当前主任务已切换为该鉴相 demo，默认参考频率为`10 kHz`，在`API/DLIA_API.h`中改`DLIA_API_REFERENCE_FREQUENCY_HZ`以匹配两路同频输入。接线时将 AD9833 输出调理后接`PA1_C / ADC1_INP1`，外部发生器输出调理后接`PA7 / ADC2_INP7`，两路必须共地、均限于 0～3.3 V。USART1 使用`PB6/PB7`、921600 8N1，每约100 ms输出一次：

```text
phase=-115.000deg raw=-114.700deg adc1=[356,666] amp=0.500V adc2=[356,666] amp=0.500V
```

此处相位含义为`ADC2相位 - ADC1相位`。`phase`是供 PI 环使用的相位差向量低通结果，`raw`是当前 512 点帧的未滤波相位差。`adc1/adc2`随后的方括号分别是该帧的原始最小/最大码值，`amp`是 IQ 解调后的峰值；`phase=invalid`仍会打印这些诊断值。对于 1 Vpp、1.65 V 偏置的正弦，原始码值应大致覆盖 356～666，`amp`应接近 0.500 V，明显偏离时先检查采样链路。双 ADC 公共 CDR 以每路 10 bit打包，幅值目前只作输入有效性判断；未做同源双通道校准前，不应把串口相位作为绝对精度结论。本板封装的`PA1`与`PA1_C`是独立焊盘：`AD9833_FSYN/CS=PA1`可保留，`ADC1_INP1=PA1_C`由 ADC 初始化打开专用模拟直连开关。当前代码不初始化 AD9833，因此 FSYNC 保持 GPIO 默认高电平，后续 PI 环路可直接接入该片。

## 双ADC相位差demo

测量范围为1 kHz到100 kHz，输入必须是同频且相位稳定的两路正弦。硬件接线（不含串口）如下：

| 信号源/前端 | STM32H750 | 用途和要求 |
| --- | --- | --- |
| 通道1调理后输出 | `PA1_C / ADC1_INP1` | 0～3.3 V单端输入，建议以1.65 V为直流偏置 |
| 通道2调理后输出 | `PA7 / ADC2_INP7` | 0～3.3 V单端输入，偏置和幅度范围与通道1一致 |
| 信号源地/前端地 | `GND/VSSA` | 两路信号源、调理电路和开发板必须共地 |
| 模拟电源 | `VDDA` | 按开发板设计接3.3 V并做好去耦，不要从信号源反向供电 |
| ADC参考正端 | `VREF+` | 使用开发板既有参考连接；测试期间保持稳定、低噪声 |
| ADC参考负端 | `VREF- / VSSA` | 使用开发板既有模拟地连接 |

若信号源输出为以0 V为中心的双极性正弦，不能直接接ADC。两路都应经过相同结构的偏置、限幅和可选RC滤波电路，例如将波形平移到1.65 V附近，并保证峰值始终位于0～3.3 V。两路电阻、电容、运放型号、走线长度和探头负载要尽量一致。

测试流程：

1. 断电完成接线，先用示波器确认`PA1_C`和`PA7`处波形均未越过0～3.3 V。DAC8830 SPI 已迁至`PB3/PD7`，可与 ADC2 的 PA7 输入并存；当前 DAC8830 波形任务占用 TIM4，不能与其他 TIM4 任务同时启动。
2. 烧录固件并复位，输入同频正弦；建议先从10 kHz、约1 Vpp、0°相差开始。
3. 观察连续输出的`freq`和`phase`，确认频率与信号源一致且相位结果稳定。
4. 依次测试1 kHz、10 kHz、50 kHz、100 kHz，以及0°、90°、-90°和接近180°相差。
5. 做同源校准时，将同一个信号一分二接入两通道，记录上述四个频点的未补偿相位差，把“实测相位差”填入`BLL/PHASE_BLL.c`中的`s_phase_calibration[]`；程序会从结果中减去该值。
6. 校准后再次扫频验收；每个测试点建议连续记录至少100帧，检查平均误差和最大误差是否都在0.5°以内。

摘要格式：

```text
freq=10000.00Hz phase=45.123deg
```

正常串口结果只输出测得的公共频率和`ADC2相位 - ADC1相位`。程序内部仍会按频率范围、输入幅度和拟合质量做有效性判断，不通过的帧不输出；相位校准表默认为全零，因此未经实物同源校准不能宣称全频段绝对误差已达到0.5°。

已知输入频率均为整数kHz，因此串口频率按最近1 kHz归整，例如`1001 Hz`显示为`1000 Hz`。未取整的测量值仍在内部用于相干窗口和正弦拟合，以吸收MCU采样时钟的比例误差；相位校准表使用归整后的标称频点。
量程判断也使用归整后的频率：例如原始测得`100369 Hz`会归整为`100000 Hz`并正常处理；归整后变为`101000 Hz`时才会按超出100 kHz拒绝。

正常启动过程不再打印`boot`或采集就绪信息，首帧计算完成后直接输出`freq`和`phase`。双ADC的单帧DMA采集通过主循环轮询`NDTR`判断完成，不依赖DMA完成回调；100 ms内`NDTR`仍未归零时才会输出`phase capture timeout`。超时行中`ndtr`用于判断DMA是否收到ADC请求，`tim_cr1/tim_cnt`用于判断TIM1是否运行，`adc_cr/adc_cfgr/adc_isr/adc_ccr`用于检查ADC触发和双模状态。

## 方波频率测量

`API/FREQ_API.*` 方波测频代码已由当前主任务启动，输入接到 `PA0 / TIM2_CH1`：

```text
信号源方波输出 -> PA0
信号源地       -> 开发板 GND
```

输入必须为 0 到 3.3 V 数字电平。5 V 或更高电压应先经过电平转换、分压或限幅；输入配置为无上下拉，信号源停止驱动时如需确定电平，可外接约 10 kΩ 下拉。

CubeMX 配置为：

- TIM2 internal clock，Prescaler 为 0，Period 为 `0xFFFFFFFF`。
- TIM2_CH1 使用 Input Capture direct mode、上升沿、DIV1、Filter 0。
- PA0 使用 AF1_TIM2、No pull、Very High speed。
- TIM2_CH1 DMA 使用 DMA1 Stream5、Peripheral-to-Memory、Word/Word、Memory Increment、Normal、High priority。
- TIM2 和 DMA1 Stream5 中断抢占优先级均为 2。

运行时模块会动态覆盖捕获预分频：低频使用 DIV1 捕获中断，高频使用 DIV8 捕获 DMA。启动时先进行约 10 ms 探测；高频 DMA 随测得频率调整长度，使测量窗口约为 200 ms，以减小输入边沿抖动造成的随机误差。最大缓冲区约 100 KB，位于 `.dma_buffer` / RAM_D2，读取前执行 DCache 失效。

应用入口：

```c
if (FREQ_API_Init() != HAL_OK)
{
    Error_Handler();
}

while (1)
{
    FREQ_API_Process();
}
```

结果可通过 `FREQ_API_GetResult()` 获取。低于 10 kHz 按 0.5 Hz 步进输出，10 kHz 到 1 MHz 按 10 Hz 步进输出。USART1 摘要示例：

```text
freq=1000.0Hz raw=999.983Hz mode=ic status=valid ticks=15000255 periods=200
freq=1000000.0Hz raw=999999.867Hz mode=dma status=valid ticks=7500001 periods=100000
```

`raw` 是校准后但未量化的结果，`freq` 是按目标分辨率整理后的结果。针对本次 1 MHz 标准输入实测为 1002814 Hz 的情况，`FREQ_FML.h` 默认设置：

```c
#define FREQ_CALIBRATION_FACTOR (0.997193896f)
```

计算方式为 `1000000 / 1002814`，该系数同时作用于低频和高频结果。更换开发板或时钟条件后，可用新的标准频率按 `标准频率 / raw测量值` 重新计算。1 MHz 上限另有 1% 判定容差：校准后轻微超过 1 MHz 时，`freq` 钳位显示为 1000000 Hz，不再变成 0；超过 1.01 MHz 时状态为 `above_range`，但 `freq` 仍显示量程上限。

当前 TIM2 时钟来自内部 HSI 派生的 75 MHz，单点校准可以减小系统比例误差，但温漂、信号源误差和输入边沿抖动仍会影响绝对准确度；需要更高精度时应改用高精度外部时钟源。

串口摘要固定每 1 s 输出一次，即使频率和状态没有变化也会继续输出；无输入时持续输出 `status=no_signal`。USART1 摘要采用中断发送，前台不等待发送完成；发送期间若生成新摘要，只保留最新的一条待发摘要。高频 DMA 模式会关闭 TIM2 和 DMA1 Stream5 NVIC，由主循环轮询 DMA NDTR 判断完成，避免持续扫频时出现 DMA 完成/中止/重启的中断竞态；切回低频输入捕获时自动恢复 TIM2 NVIC。

## AD9226 并行采集验证 Demo

当前主任务沿用参考工程的采集方式：TIM1_CH1 输出 1 MHz 采样时钟，DCMI 按 12 位并行数据接收，DMA2 Stream1 每帧采集 4096 点。采集缓冲区位于 RAM_D2，已适配当前工程开启的 D-Cache。

数据与时钟接线：

| AD9226 | STM32H750 |
| --- | --- |
| D0、D1、D2、D3 | PC6、PC7、PC8、PC9 |
| D4、D5、D6、D7 | PE4、PB6、PE5、PE6 |
| D8、D9、D10、D11 | PC10、PC12、PB5、PD2 |
| CLK | PA8 / TIM1_CH1 |
| GND | 开发板 GND |

还需要三组回接线：

- PA8 同时接 AD9226 CLK 和 PA6/DCMI_PIXCLK。
- PB1 接 PA4/DCMI_HSYNC。
- PB2 接 PB7/DCMI_VSYNC。

`.ioc` 中的主要配置为：DCMI Hardware Sync、12-bit、PIXCLK Falling Edge、HSYNC/VSYNC Active High；DMA2 Stream1 使用 Peripheral-to-Memory、Word/Word、Normal、Very High Priority、FIFO Full、Memory Burst INC4。TIM1 的计数时钟为 240 MHz，PSC=`0`、ARR=`239`、CCR1=`120`，对应 1 MHz、50% 占空比。

USART3 使用 PB10/PB11、115200 8N1，TX 通过 DMA1 Stream4 输出。USART1 为释放 PB6/PB7，在 AD9226 `.ioc` 配置下临时改到 PA9/PA10，板载 USB 转串口通常不能用于该 demo。

建议验收顺序：

1. 断电接线并确认 AD9226 模块供电、参考和数字电平符合模块要求，所有设备共地。
2. 上电后用示波器检查 PA8 和 PA6，均应看到约 1 MHz 时钟。
3. 先输入 0 V 或已知直流，观察串口的 `min/max/mean`，检查数据线位序和稳定性。
4. 输入约 1 kHz 正弦。当前算法只接受约 800～1200 个采样点的周期，默认有效范围约为 833～1250 Hz。
5. 正常结果包含 `frequency`、`thd`、`u1`～`u5`、`min/max/mean` 和 `periods`；`dcmi_capture_error:1` 表示采集或同步回路失败，`thd_valid:0` 表示有数据但不满足幅度或周期条件。

AD9226 模块按参考工程视为 12 位 offset-binary、标称 10 V 总跨度。基波和谐波电压使用 `10 V / 4096` 换算，该比例及前端增益必须在实物上重新校准，不能把当前编译结果视为幅值精度验收。

## 串口调试

串口发送入口：

- `Usart_Send_Computer(&huart1, "msg\r\n")`
- `Usart_Send_ADC_Data(data, &huart1, len)`

波特率为 921600。大量打印原始数组会占用明显时间，比赛时如果需要实时控制，应减少打印或只打印摘要。

## CMake 和依赖

本工程的命令行编译和烧录统一使用 `Tools/build.ps1` 与 `Tools/flash.ps1`。首次使用前，将 `Tools/config/local.ps1.example` 复制为被 Git 忽略的 `Tools/config/local.ps1`，填写本机 STM32CubeIDE for VS Code 的 Cube CMake/运行时目录，以及 OpenOCD 路径。常规增量构建为 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1`；缓存失效时在末尾追加 `-Reconfigure`。仅在明确需要烧录时将脚本改为 `Tools\flash.ps1`，它会先构建并验证 ELF，再执行 OpenOCD 的 programming、verify、reset。

本项目使用本机 STM32CubeH7 的 CMSIS-DSP 静态库：

```cmake
$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1
```

如果换机器后配置失败，优先确认 `STM32Cube_FW_H7_V1.12.1` 是否存在，或在 CMake cache 中设置 `STM32CUBE_FW_H7_PATH`。

## AD9910 API

AD9910 代码已接入当前主任务，默认调用 `AD9910_API_Init()` 和 `AD9910_API_OutputSine(100000U, 500U)`。也可使用以下接口：

- `AD9910_API_StartSineByAmplitudeCode(frequency_hz, amplitude_code)`：以原始 14 位 ASF（`0～0x3FFF`）启动单 Profile 正弦，适合标定与手动调试；连续更新只重写 Profile 0。
- `AD9910_API_StartWaveform(waveform, frequency_hz, amplitude_mvpp)`：`amplitude_mvpp` 指 AD9910 模块直接输出端；波形可选 `AD9910_API_WAVE_SINE`、`AD9910_API_WAVE_TRIANGLE` 和 `AD9910_API_WAVE_SQUARE`。
- `AD9910_output_sine(hz, mvpp)`：`mvpp` 指固定十倍后级放大后的目标幅度。其满量程标定值与后级增益定义在 `HDL/AD9910_Constants.h`；换用输出链路前必须重新标定，不能将该接口的幅度语义用于未接十倍后级的直接输出。

测试接线与流程如下：

1. 断电接线，开发板与 AD9910 模块必须共地；连接 `MRT=PA6`、`CSN=PD4`、`SCK=PA8`、`SDI=PA12`、`IUP=PD5`、`PF0=PG11`、`PF1=PG9`、`PF2=PG7`。
2. 按 AD9910 模块要求提供其电源和系统时钟，并将示波器或频谱仪接到模块已配置的模拟输出端；仪器地与系统地相连。勿将 5 V 逻辑电平直接接入 H750 GPIO。
3. 用户明确要求编译时执行 `Tools/build.ps1`；明确要求烧录时执行 `Tools/flash.ps1`，然后复位开发板。
4. 从应用代码显式调用待测 API。`AD9910_API_StartWaveform()` 的参数越界会返回 `HAL_ERROR`；`AD9910_output_sine()` 的非法参数会直接返回。
5. 测量直接输出时，AD9910 输出端接示波器，设为 1 MΩ 高阻输入、DC 耦合，确认频率、波形和 Vpp。`AD9910_API_StartWaveform()` 三种波形幅度均允许 0～724 mVpp；更换模块、输出网络或十倍后级后均需重新标定。
6. 若无输出，先检查模块电源/时钟、共地、输出端跳线及上述 8 根控制线，再用逻辑分析仪确认 `CSN/SCK/SDI/IUP` 在复位后有写寄存器时序。

正弦波使用单 Profile DDS，支持 1 Hz～400 MHz；三角波和方波使用 RAM 连续回放，支持约 77 Hz～5 MHz。程序会在 50～1024 点之间搜索频率误差最小的组合，同误差时优先选择更多点；100 kHz 使用 625 点、地址步进率 4，频率保持精确的同时明显减小阶梯。频率和幅度均依赖模块的实际系统时钟及模拟输出网络，烧录后必须以示波器实测为准。

非正弦路径使用 RAM polar 数据和正弦 DDS 映射，以 90°/270°相位表示正负样本。因此 CFR1[16] 必须保持为 1；若误选余弦输出，90°相位会落在零点，表现为无输出或幅度异常。

## ADS8688 四通道 + VOFA+ 任务

当前主任务使用 ADS8688 手动通道轮询内部通道 `0`～`3`，对应模块标注的 `CH1`～`CH4`。四路均配置为 `+-10.24 V` 量程。每轮对一个通道写入 `MAN_Ch_n` 命令，再执行一次 4 字节零数据读取，依次得到 CH1～CH4；这用于规避当前模块在软件 SPI 自动扫描模式下四路均返回满量程的问题。

| ADS8688 模块 | STM32H750 | 说明 |
| --- | --- | --- |
| `CS` / `nCS` | `PB12` | 片选，低有效 |
| `RST/PD` | `PC4` | 复位/低功耗，demo 保持高电平 |
| `SCLK` | `PB13` | GPIO 软件 SPI 时钟 |
| `SDO` | `PB14` | ADC 数据输出，GPIO 输入 |
| `SDI` | `PB15` | MCU 命令输出，GPIO 输出 |
| `DAISY_IN` | `GND` | 单片使用时固定拉低，不要悬空 |
| `CH1` | 信号 1 | 内部通道 0，默认允许 -10.24 V～+10.24 V |
| `CH2` | 信号 2 | 内部通道 1，默认允许 -10.24 V～+10.24 V |
| `CH3` | 信号 3 | 内部通道 2，默认允许 -10.24 V～+10.24 V |
| `CH4` | 信号 4 | 内部通道 3，默认允许 -10.24 V～+10.24 V |
| `VIN` | 模块规格允许的 3.3 V 或 5 V 电源 | 以模块丝印/说明书为准，不接 MCU GPIO |
| 模块 `GND` | 开发板 GND/信号源 GND | 三者必须共地 |

当前 ADS8688 使用 GPIO 软件 SPI：`PB13=SCLK`、`PB14=SDO`、`PB15=SDI`，片选仍为 `PB12`。时序与参考驱动一致：发送字节时在 SCLK 上升沿输出，读取字节时在 SCLK 上升沿采样。主任务不初始化 SPI2。

VOFA+ 配置：

1. 选择 USART1 对应串口，设为 921600 baud、8N1、无流控。
2. 协议引擎选择文本 `FireWater`。
3. 每帧发送 CH1～CH4，格式为 `samples:<ch1>,<ch2>,<ch3>,<ch4>\r\n`，例如 `samples:1.000000,2.000000,3.000000,4.000000\r\n`。这符合 FireWater 的 `<any>:ch0,ch1,...\n` 格式；四个值依次对应模块 `CH1`、`CH2`、`CH3`、`CH4`。除上电 `ok` 心跳外，不要在同一串口中混入其他文本日志。
4. UART 通过 DMA 异步发送；串口忙时只保留最新一帧，因此不会阻塞采集。程序每采集 25 组四路样本发送一行。本 demo 优先用于验证通道顺序、量程和波形，软件 SPI 轮询读取下不承诺固定的每通道 100 kS/s。

测试流程：

1. 断电完成上表接线，核对 ADS8688 模块电源和参考电压要求，先不加输入信号。
2. 烧录 Debug 固件并复位，打开 VOFA+ 后观察 CH1；CH1 接 0 V 时应接近 0 V，对应原始码约 `32768`。
3. 向 CH1 输入已知直流正、负电压（例如 +1.000 V 和 -1.000 V），确认极性和比例。
4. 向 CH1 输入 10 kHz、1 Vpp 正弦波，检查显示的幅度和偏置。VOFA+ 横轴是输出帧序号，显示采样间隔为 250 us；采集实际调度仍为每通道 10 us。
5. 若 VOFA+ 无数据，先检查 `PB12/PB13/PB15` 是否有 CS、SCK 和 SDI 时序，再检查 `PB14` 是否返回数据。上电应先收到 `ok`；有 `ok` 但没有四通道 `samples:` 数据时，重点检查 ADS8688 的软件 SPI 和模块供电。

## 淘晶驰 TJC1060X570_011C_I 串口屏 Demo

当前主任务使用 USART3 驱动淘晶驰 X5 系列 `TJC1060X570_011C_I`（1024×600、电容触摸）。STM32 每秒刷新一个数字控件，屏幕按钮通过串口回传命令，实现暂停/继续和清零；USART1 继续连接板载 USB 转串口用于调试。

### 1. 接线与供电

| 串口屏 | STM32H750 | 说明 |
| --- | --- | --- |
| `RX` | `PB10 / USART3_TX` | 发送接收交叉连接 |
| `TX` | `PB11 / USART3_RX` | 发送接收交叉连接 |
| `GND` | 开发板 `GND` | 必须共地 |
| `5V` | 独立 5 V 稳压电源 | 不建议由 MCU 的 3.3 V 引脚供电 |

使用独立 5 V 电源给屏供电时，电源地、屏幕地和 STM32 地仍须相连。确认屏幕接口当前配置为 **TTL**，不要在 RS232 模式下直接连接 MCU。X3/X5 某些接口配置下 TX 电平可能较高；首次连接建议先测量，必要时按官方建议在“屏 TX → MCU RX”之间串联 1 kΩ 电阻。

### 2. 在 USART HMI 中创建界面

1. 新建工程，在“设备”中选择完整型号 `TJC1060X570_011C_I`，显示方向按实物安装选择，字符编码可保持默认；本 Demo 由 MCU 下发的文本只有 ASCII，不涉及中文编码。
2. 将默认页面重命名为 `main`。
3. 打开工程中的 `program.s`，写入以下上电配置；`page` 必须放在最后，因为它后面的语句不会执行：

```text
bauds=115200
recmod=0
bkcmd=0
page main
```

4. 导入一个包含英文字母和数字的字库。
5. 在 `main` 页面放置以下控件，控件名称必须完全一致，并把 `vscope` 设为“全局”：

| 类型 | 控件名 | 建议初始内容 | 用途 |
| --- | --- | --- | --- |
| 文本 | `t0` | `WAITING` | 显示 `RUNNING` / `PAUSED` |
| 数字 | `n0` | `0` | 显示秒计数 |
| 按钮 | `b0` | `PAUSE` | 暂停或继续 |
| 按钮 | `b1` | `RESET` | 计数清零 |

6. 双击 `b0`，在“弹起事件”中加入：

```text
printh 5A A5 01 FF FF FF
```

7. 双击 `b1`，在“弹起事件”中加入：

```text
printh 5A A5 02 FF FF FF
```

8. 点击“编译”，先在软件模拟器中检查页面布局；再生成 TFT 文件并通过串口或 TF 卡下载到屏幕。工程型号必须和实物型号一致，否则会提示 `model does not match`。

### 3. 通讯参数与验收

- 波特率：115200
- 数据格式：8N1
- 流控：无
- TJC 字符串指令结束符：十六进制 `FF FF FF`

烧录本工程固件并给屏幕上电。约 0.8 秒后应自动进入 `main` 页面，`t0` 显示 `RUNNING`，`n0` 每秒加 1。点击 `b0` 后状态变为 `PAUSED` 且数字停止，再次点击恢复；点击 `b1` 后数字归零。

STM32 发送文本和数值的实际指令分别类似：

```text
main.t0.txt="RUNNING" FF FF FF
main.n0.val=10 FF FF FF
```

代码入口在 `API/TJC_HMI_API.c`，通用协议封装在 `HDL/TJC_HMI.c`。以后要显示测频或相位结果，可直接调用 `TJC_HMI_SetValue()` 或 `TJC_HMI_SetText()`；中文文本必须保证 MCU 源文件字节编码、USART HMI 工程编码和字库编码一致。

若没有显示，依次检查：屏幕是否已下载正确 TFT、TTL/RS232 模式、115200 8N1、TX/RX 是否交叉、是否共地、控件名和 `vscope`。当前串口屏只能接 USART3 的 PB10/PB11，不要再并联到 USART1 的 PB6/PB7。
# ADC2 FFT Measurement Task

Current `main.c` runs one ADC2 FFT measurement and waveform classification while
the AD9833 and AD9910 DDS tasks remain enabled. ADC2 input is `PA7 / ADC2_INP7`;
keep the voltage within 0-3.3 V and connect the signal-source ground to board
ground. ADC2 is triggered by internal TIM1 TRGO and captured through DMA1
Stream1. Do not enable the on-chip DAC/TIM4 task or the DAC UART command task
with this measurement task.

USART1 uses `PB6` TX and `PB7` RX at 921600 8N1. Disconnect any AD9226 wiring
from PB6/PB7 before connecting the USB serial adapter. After reset the firmware
captures one coarse 4096-point frame, then one final 4096-point frame near
360-409.6 kS/s. The peak search range is 1-100 kHz. The final UART payload is:

```text
raw begin n=4096 fs=409556.312
raw:<10-bit-code>
...
raw end
spectrum begin n=2048 df=<Hz>
fft:<bin>,<amplitude-v>
...
spectrum end
result freq=<Hz> bin=<bin> fs=<Hz> df=<Hz> wave=<type> spec_sine=<score> spec_triangle=<score> spec_square=<score> h3=<ratio> h5=<ratio> closure=<cycles> class_fs=<Hz> class_spp=<count> class_closure=<cycles>
```

After the final frequency frame, the task captures one additional 4096-point
classification frame. It selects a TIM1 rate near an integer multiple of the
measured frequency, using 8 to 1024 samples per period and minimizing residual
cycle closure after the hardware timer divider is rounded. The classification
sample-rate ceiling is 2 MS/s, which remains above the measurement-frame rate
while retaining timing margin for ADC2, DMA, and the board analog front end.
The frequency result continues to come only from the preceding final frequency
frame. If ADC2 reports an error during any capture stage, USART1 sends
`adc2fft error stage=coarse|final|classify adc=<code>` instead of waiting
silently. Each capture stage also prints `adc2fft stage=coarse|final|classify`;
if a frame has not completed after 100 ms, the same stage error diagnostic is
sent. A square-wave source must still be conditioned to 0-3.3 V at PA7, with no
negative undershoot or positive overshoot.

The final `result` line reports `wave`, three spectral-profile scores, and
diagnostic `h3`/`h5` ratios. Classification reads the first six visible odd
harmonics (1st through 11th), using a three-bin neighborhood around each expected
harmonic to tolerate residual leakage. The fundamental is used only to normalize
the other harmonics; it is excluded from triangle/square profile matching so it
cannot make every waveform appear sine-like. The residual odd-harmonic profile
is compared with triangle and square profiles, while the sine score decays
smoothly with total odd-harmonic distortion. Individual harmonic attenuation
ratios are not used as strict thresholds. `wave=unknown` is returned for a small
signal, weak best match, or an ambiguous best match. This classification does
not change the peak-search or frequency-measurement path.

The FFT search includes the bins that straddle both configured frequency limits.
This is required because the actual TIM1 rate is derived from an integer divider;
for example, 1 kHz can fall just above the nominal lower-edge bin rather than
exactly on it. Excluding that bin would make the classifier follow a leakage
side lobe instead of the fundamental.

Only the final frame and its nonredundant 2048-bin spectrum are sent. `df` is
no more than 100 Hz with the current clock configuration. UART output is
blocking only after sampling stops, so the report needs a visible serial
interval at 921600 baud. This is compiled but not yet flashed or
hardware-verified.
# FFT BLL Input Configuration

`ADC2_FFT_BLL_Analyze(raw_samples, &input, &result)` can analyze a captured
4096-sample `uint16_t` frame from another ADC port. Set `input.sample_rate_hz`,
`input.volts_per_lsb`, `input.search_min_frequency_hz`,
`input.search_max_frequency_hz`, `input.sample_bit_width`, and
`input.sample_encoding`. Supported encodings are `ADC2_FFT_SAMPLE_UNSIGNED`,
`ADC2_FFT_SAMPLE_OFFSET_BINARY`, and `ADC2_FFT_SAMPLE_TWOS_COMPLEMENT`.

For a 12-bit 0-3.3 V internal ADC, use `volts_per_lsb = 3.3f / 4095.0f` and
`ADC2_FFT_SAMPLE_UNSIGNED`. For signed data, `volts_per_lsb` must use the signed
code scale. The caller keeps responsibility for ADC capture, DMA, cache handling,
and calibration; FFT BLL only receives its completed array and descriptor.
# ADC2 FFT Single-Shot Mode

The active ADC2 FFT task runs once after reset. It captures a coarse frame,
chooses the coherent final capture, then sends the final raw data, spectrum, and
frequency result. ADC2 sampling remains stopped throughout UART output and does
not restart after the report is complete.
# SUPER_FFT Main Task

The active main task uses `SUPER_FFT` with ADC3 (`PC2_C / ADC3_INP0`), DMA1
Stream3, and internal TIM6 TRGO. It does not initialize ADC2 or TIM1; the ADC2
FFT startup and process calls remain commented in `main.c`. `SUPER_FFT` performs
its own 4096-point acquisition sequence and exposes the completed result through
`SUPER_FFT_IsReady()` and `SUPER_FFT_GetFrequencyHz()`.

USART1 is initialized only for measurement changes. At 921600 8N1 it sends:

```text
freq=<measured-Hz>Hz
```

`SUPER_FFT` restarts automatically after every completed measurement. A line is
sent on the first completed result and then only when the result rounded to the
nearest 1 Hz differs from the last printed result.

USART1 output uses `HAL_UART_Transmit_IT()`. The FFT task never waits for serial
transfer completion. If a newer changed frequency arrives while USART1 is busy,
the single pending slot is replaced with that newest value.

`SUPER_FFT` uses a dedicated windowed direct-DFT FML for the searched bins. It
keeps the same 4096-point window, power accumulation, and peak interpolation as
the previous CFFT path, but does not link the CMSIS 4096-point CFFT tables.
Measurement updates are therefore slower, while frequency resolution and the
measurement algorithm remain unchanged.

The initial 400 kS/s FFT searches 10 Hz through 100 kHz. Results below 12 kHz
automatically enter the 40 kS/s low-frequency coarse and 1 Hz fine-scan path,
which covers 10 Hz through 12 kHz. The small overlap lets an actual 10 kHz
signal enter the fine path even when the high-rate coarse FFT lands slightly
above the 10 kHz boundary.

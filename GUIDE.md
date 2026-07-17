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
6. 编译验证：`cmake --build --preset Debug`。

## DAC8830 使用示例

```c
#include "DAC8830.h"

DAC8830_Init();
DAC8830_SelectBipolar10V();
DAC8830_SetVoltageMvA(1000);   // OUTA = +1.000 V
DAC8830_SetVoltageMvB(-1000);  // OUTB = -1.000 V
DAC8830_WriteBoth(32768U);     // 两路直接写 16 bit 码值
```

默认输出模式是 `DAC8830_OUTPUT_BIPOLAR_10V`。如果模块跳线不是 ±10 V，请调用对应选择函数，或用 `DAC8830_SetRangeMv(min, max)` 设置自定义量程。

当前 DAC8830 正弦 demo 使用 `HDL/DAC8830_DMA.*`，由 TIM4 触发 DMA：Update 拉低 `PE2/PE0`，CH1 把 16-bit 码值写入 `SPI1->TXDR`，CH2 拉高 `PE2/PE0` 锁存输出。250 点、10 kHz 对应 2.5 MS/s，SPI1 使用 16-bit 帧，SCK 目标约 60 Mbit/s；DMA 源表必须放在 `.dma_buffer` / RAM_D2。

## 片上 DAC 波形输出

`FML/DAC_FML.*` 提供：

- `DAC_Waveform_Start(type, frequency_hz, vpp, offset_v)`
- `DAC_Waveform_Apply(type, frequency_hz, vpp, offset_v)`
- `DAC_Waveform_Stop()`

波形缓冲区长度为 256 点，输出频率由 `TIM4` 更新触发控制。输出电压限制在 0 到 3.3 V。

## ADC + FFT 流程

当前采样处理链路：

1. `ADC_API_Init()` 配置 TIM1 Update TRGO、执行 ADC 单端 offset 校准并启动 DMA。
2. TIM1 以约 31030.2027 SPS 触发 ADC1，DMA one-shot 采集 1024 点到 `.dma_buffer` / RAM_D2；固定 1 kHz 输入时一帧约包含 33.000107 个周期，接近相干采样。
3. DMA 完成后停止 TIM1 和 ADC DMA；仅在 DCache 实际开启时执行缓存失效，再把 16 bit 码值换算为 0 到 3.3 V。当前工程未启用 DCache，RAM_D2 缓冲区不执行缓存维护。
4. 数据去均值、乘 Hann 窗并执行 1024 点 FFT；频谱幅值按 Hann 相干增益修正，不再对复数模值错误开方。
5. 先在 FFT 主峰左右半个 bin 内对 Hann 加窗 DFT 做连续频率细化，再直接在细化后的 `f0`、`2f0`～`5f0` 处估计幅值，避免非整数周期采样时固定三点频带积分产生不一致的主瓣截断误差；严格按题目公式 `sqrt(U2^2 + U3^2 + U4^2 + U5^2) / U1` 计算 THD。
6. 依据 THD、偶次谐波以及 3/5 次谐波比例识别 `sine`、`square`、`triangle`；条件不足时输出 `unknown`。
7. USART1 先输出 1024 点原始波形，再输出 512 点半边频谱，最后输出结果；上电只采集并打印一帧，完成后不自动重启。

CubeMX/硬件配置：

- `PA1_C / ADC1_INP1`，16 bit、single-ended、采样时间 8.5 cycles。
- ADC 外部触发为 `TIM1 TRGO` rising，DMA1 Stream0 为 Peripheral-to-Memory、Word/Word、Memory Increment、Normal、High priority。
- TIM1 internal clock，Prescaler=0、运行时 Period=2416、Master Output Trigger=Update Event；当前 APB2=37.5 MHz、TIM1=75 MHz，因此实际采样率为 `75 MHz / 2417 = 31030.2027 Hz`。1 kHz 基波约位于第 33 个 bin，五次谐波约位于第 165 个 bin，低于 15515.10 Hz Nyquist 上限。
- 当前 `.ioc` 已保存 `TIM_TRGO_UPDATE`，但 CubeMX 生成值可能与运行值不同；`ADC_FML` 启动时会再次设置 Update TRGO，并按目标采样率自动设置 TIM1 ARR。若希望 CubeMX 配置也保持一致，可手动将 TIM1 Period 改为 2416。

输入必须限制在 0～3.3 V 并与开发板共地。双极性信号需要先增加约 1.65 V 偏置，推荐测试条件为约 2 Vpp、1.65 V offset；负电压或 5 V 信号不能直接接入 ADC。

串口帧格式：

```text
frame begin fs=31030.20 n=1024
raw begin
0,1.650123
...
raw end
spectrum begin
0,0.000123
1,0.001234
...
spectrum end
result status=valid freq=1000.00Hz thd=11.809% wave=triangle h2=0.0000 h3=0.1111 h5=0.0400 fs=31030.20Hz
frame end
```

原始波形每行为 `采样点序号,电压V`，半边频谱每行为 `bin序号,幅值V`；不逐 bin 打印频率。输入信号频率只在最后的 `result freq=...Hz` 中输出。

当前配置专用于固定 1 kHz 输入，FFT bin 间隔约为 30.303 Hz，1024 点窗口约为 32.9999 ms。由于 TIM1 只能使用整数分频，在不改变 75 MHz 定时器时钟的情况下不能做到绝对相干；当前 2417 分频已将单帧周期误差压到约 0.000107 周期。若输入频率不再固定为 1 kHz，应重新选择采样率。无信号时输出 `status=no_signal`；频率过低或可用谐波不足时输出 `status=limited`、`thd=invalid`。

应用入口：

```c
if (ADC_API_Init() != HAL_OK)
{
    Error_Handler();
}

while (1)
{
    ADC_API_Process();
}
```

## 方波频率测量

工程仍保留 `API/FREQ_API.*` 测频 demo（当前主任务未启动），启用后输入接到 `PA0 / TIM2_CH1`：

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

串口摘要固定每 1 s 输出一次，即使频率和状态没有变化也会继续输出；无输入时持续输出 `status=no_signal`。高频 DMA 模式会关闭 TIM2 和 DMA1 Stream5 NVIC，由主循环轮询 DMA NDTR 判断完成，避免持续扫频时出现 DMA 完成/中止/重启的中断竞态；切回低频输入捕获时自动恢复 TIM2 NVIC。

## 串口调试

串口发送入口：

- `Usart_Send_Computer(&huart1, "msg\r\n")`
- `Usart_Send_ADC_Data(data, &huart1, len)`

波特率为 921600。大量打印原始数组会占用明显时间，比赛时如果需要实时控制，应减少打印或只打印摘要。

## CMake 和依赖

本项目使用本机 STM32CubeH7 的 CMSIS-DSP 静态库：

```cmake
$ENV{USERPROFILE}/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1
```

如果换机器后配置失败，优先确认 `STM32Cube_FW_H7_V1.12.1` 是否存在，或在 CMake cache 中设置 `STM32CUBE_FW_H7_PATH`。

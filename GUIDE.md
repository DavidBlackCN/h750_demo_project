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

1. CPU运行在480 MHz，TIM1计数时钟为240 MHz。
2. 频率阶段以1 MS/s同步采集4096组ADC1/ADC2数据，4096点FFT粗定位后通过正弦拟合得到公共频率。
3. 相干阶段根据公共频率联合搜索TIM1整数分频、3072～4096点采样长度和整数周期数，采样率保持在1～1.1 MS/s并优先接近1 MS/s，同时使窗口首尾相位闭合误差最小。
4. 用搜索得到的实际采样率和第一阶段公共频率拟合两路相位，计算`ADC2相位 - ADC1相位 - 校准值`。
5. 完成相位拟合后只输出频率和相位差，然后重新回到1 MS/s测频阶段。

相位帧会同时检查两路三次谐波与基波幅度比。两路比值都不低于0.04时，按三角波/强谐波波形处理：先在一个周期内搜索两路归一化互相关峰，再用黄金分割细化到小数采样点延迟，最后把时延换算为相位。未检测到强三次谐波时仍使用原正弦最小二乘相位，因此正弦测量路径不变。三角波互相关低于0.98的帧不输出。

当前DMA采用normal模式，频率帧和相位帧使用同一缓冲区但分时采集。4096点FFT只负责先测频，相位差只使用后续相干采样帧。

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

1. 断电完成接线，先用示波器确认`PA1_C`和`PA7`处波形均未越过0～3.3 V。当前相位demo将PA7用作ADC2输入，不能同时启用使用PA7的DAC8830 SPI输出。
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

当前主任务运行 `API/FREQ_API.*` 测频 demo，输入接到 `PA0 / TIM2_CH1`：

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

## AD9910 单 Profile demo

AD9910 demo代码仍保留，但当前主任务不启动AD9910。需要恢复该demo时，可按以下步骤操作：

1. 断电接线，开发板与 AD9910 模块必须共地；连接 `MRT=PA6`、`CSN=PD4`、`SCK=PA8`、`SDI=PA12`、`IUP=PD5`、`PF0=PG11`、`PF1=PG9`、`PF2=PG7`。
2. 按 AD9910 模块要求提供其电源和系统时钟，并将示波器或频谱仪接到模块已配置的模拟输出端；仪器地与系统地相连。勿将 5 V 逻辑电平直接接入 H750 GPIO。
3. 执行 `cmake --build --preset Debug`，烧录生成的固件，然后复位开发板。
4. 打开 USART1（PB6/PB7，921600、8N1），应看到 `boot` 和 `ad9910 triangle: 1MHz amp=1638 nominal=200mVpp`。
5. 示波器设为 1 MΩ 高阻输入、DC 耦合，确认约 1 MHz 的 50 阶近似三角波。若模块满量程为约 2 Vpp，`1638` 约为 200 mVpp；实测值不符时按 `新码值 = 1638 × 目标Vpp / 实测Vpp` 换算 `AD9910_TriangleWave_Init()` 的参数，并限制在 0 到 16383。
6. 若无输出，先检查模块电源/时钟、共地、输出端跳线及上述 8 根控制线，再用逻辑分析仪确认 `CSN/SCK/SDI/IUP` 在复位后有写寄存器时序。

三角波由 `AD9910_TriangleWave_Init(1638U)` 配置。函数以 50 点极性 RAM 数据生成三角波，系统时钟为 1 GHz 时，RAM 地址步进率 5 对应 50 MHz 回放率，故波形频率为 1 MHz。频率和幅度均依赖模块的实际系统时钟及模拟输出网络，烧录后必须以示波器实测为准。

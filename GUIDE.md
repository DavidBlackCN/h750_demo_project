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

`API/FREQ_API.*` 测频 demo 代码仍保留，但当前主任务不启动。恢复该 demo 后，输入接到 `PA0 / TIM2_CH1`：

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

## AD9910 单 Profile demo（保留代码）

该独立波形 demo 代码仍保留，但当前主程序已切换到 G 题。恢复旧 demo 时可在 `Core/Src/main.c` 使用以下三个宏：

```c
#define AD9910_DEMO_FREQUENCY_HZ       100000UL
#define AD9910_DEMO_AMPLITUDE_MVPP     500U
#define AD9910_DEMO_WAVEFORM           AD9910_API_WAVE_TRIANGLE
```

波形可选 `AD9910_API_WAVE_SINE`、`AD9910_API_WAVE_TRIANGLE` 或
`AD9910_API_WAVE_SQUARE`，无需手工换算 AD9910 幅度码。测试步骤如下：

1. 断电接线，开发板与 AD9910 模块必须共地；连接 `MRT=PA6`、`CSN=PD4`、`SCK=PA8`、`SDI=PA12`、`IUP=PD5`、`PF0=PG11`、`PF1=PG9`、`PF2=PG7`。
2. 按 AD9910 模块要求提供其电源和系统时钟，并将示波器或频谱仪接到模块已配置的模拟输出端；仪器地与系统地相连。勿将 5 V 逻辑电平直接接入 H750 GPIO。
3. 执行 `cmake --build --preset Debug`，烧录生成的固件，然后复位开发板。
4. 打开 USART1（PB6/PB7，921600、8N1），启动成功后应看到 `ad9910 waveform started`；参数越界则显示 `ad9910 waveform parameter invalid`。
5. 旧 demo 的 mVpp 参数指 AD9910 模块直接输出。复核该基准时应旁路 G 题的 10 倍后级后接示波器，设为 1 MΩ 高阻输入、DC 耦合；三种波形的直接输出幅度范围为 0～724 mVpp。更换模块或输出网络后需要重新标定。
6. 若无输出，先检查模块电源/时钟、共地、输出端跳线及上述 8 根控制线，再用逻辑分析仪确认 `CSN/SCK/SDI/IUP` 在复位后有写寄存器时序。

正弦波使用单 Profile DDS，支持 1 Hz～400 MHz；三角波和方波使用 RAM 连续回放，支持约 77 Hz～5 MHz。程序会在 50～1024 点之间搜索频率误差最小的组合，同误差时优先选择更多点；100 kHz 使用 625 点、地址步进率 4，频率保持精确的同时明显减小阶梯。频率和幅度均依赖模块的实际系统时钟及模拟输出网络，烧录后必须以示波器实测为准。

非正弦路径使用 RAM polar 数据和正弦 DDS 映射，以 90°/270°相位表示正负样本。因此 CFR1[16] 必须保持为 1；若误选余弦输出，90°相位会落在零点，表现为无输出或幅度异常。

## G题完整应用：USART1与USART3同时工作

当前主程序同时运行USART1 USB调试入口和USART3串口屏入口。两路控制共享同一套G题业务接口，按事件先后顺序更新AD9910；不自动扫频。幅度统一按“十倍放大后满量程对应ASF 0x3FFF”换算，当前实测标定值为7646.0 mVpp。

### 完整题目模式

```c
#define G_APP_SELECTED_REQUIREMENT  G_CONSOLE_API_REQUIREMENT_ALL
```

当前 `main.c` 将 `G_APP_SELECTED_REQUIREMENT` 设为完整题目模式。上电不启动任何要求，主循环同时等待USART1命令和USART3串口屏操作；只有用户明确启动后才配置AD9910输出。要求（3）和要求（4）的无屏版本均已完成实物验收。

上电完成增益表自检后保持空闲，不运行任何要求，也不主动配置AD9910。用户可通过USB串口命令或串口屏菜单启动输出。

### 串口命令

| USB串口输入 | 行为 |
| --- | --- | --- |
| `r2 1000` | 输出1 kHz、最终端口3 Vpp正弦；频率允许100 Hz～1 MHz，步长100 Hz |
| `r3` | 按1 kHz增益点反算探究装置幅度，使已知模型电路目标输出为2.0 Vpp |
| `r4 1000 2.0` | 频率允许100～3000 Hz/步长100 Hz，目标允许1.0～2.0 Vpp/步长0.1 V |

要求 3/4 使用 `BLL/G_MODEL_BLL.c` 中 30 个只读增益点，数组依次对应 100、200、……、3000 Hz，并统一使用以下算法：

```text
source_Vpp = target_Vpp / known_model_gain[index]
ASF = 0x3FFF * source_mVpp / AD9910_OUTPUT_FULL_SCALE_MVPP
index = (frequency_hz - 100) / 100
```

成功后USART1只返回核心数据：要求编号、频率、目标Vpp、AD9910直接输出Vpp、10倍后级输出Vpp和ASF。只有要求3/4显示已知模型增益；要求2没有连接已知模型，模型增益始终显示 `N/A`，其3.0 Vpp目标直接对应探究装置末级输出。非法频率、步长或目标档位只返回错误，不重新配置AD9910。

### 上板验收注意事项

1. AD9910 模块、10 倍放大板、已知模型电路、示波器和开发板必须共地。
2. 所有文档中的“源端输出”均指 10 倍放大后的探究装置最终连接器；AD9910 芯片直接输出仅为该值的十分之一。
3. 示波器负载必须固定使用同一种设置（建议先用 1 MΩ 高阻），不要在校准和验收间切换 1 MΩ/50 Ω。
4. 要求 2 先人工覆盖 100 Hz、1 kHz、3 kHz、10 kHz、100 kHz、500 kHz、1 MHz，确认频率误差、3 Vpp 幅度及失真。
5. 要求 3 验证已知模型输出为 2.000 Vpp ±5%；要求 4 至少覆盖四角点和 1 kHz/2.0 V。固件没有读取模型输出的反馈通道。
6. 当前状态只表示已编译并完成公式复核；实际 10 倍放大、负载、模型元件误差和全频段幅度仍需上板测试。

## ADS8688 CH1 + VOFA+ demo

ADS8688 demo 代码仍保留，但当前主任务不启动。恢复该 demo 后，程序使用 ADS8688 的内部通道 0 采集模块上标注的 CH1，默认量程为 ±10.24 V。硬件接线如下：

| ADS8688 模块 | STM32H750 | 说明 |
| --- | --- | --- |
| `CS` / `nCS` | `PB12` | 片选，低有效 |
| `RST/PD` | `PC4` | 复位/低功耗，demo 保持高电平 |
| `SCLK` | `PB13 / SPI2_SCK` | SPI 时钟 |
| `SDO` | `PB14 / SPI2_MISO` | ADC 数据输出 |
| `SDI` | `PB15 / SPI2_MOSI` | MCU 命令输出 |
| `DAISY_IN` | `GND` | 单片使用时固定拉低，不要悬空 |
| `CH1` | 待测模拟信号 | 默认允许 -10.24 V～+10.24 V |
| 模块 `GND` | 开发板 GND/信号源 GND | 三者必须共地 |

SPI2 配置为 Master、Full Duplex、8 bit、MSB first、CPOL low、CPHA 2-edge、软件 NSS、NSS pulse disabled。SPI123 使用 136 MHz PLL3P，SPI2 分频 `/8`，SCK 为 17 MHz。初始化和寄存器访问使用 HAL SPI，1000 点高速采样使用专用 SPI2 FIFO 轮询路径，以避免每点进入完整 HAL 调用。

VOFA+ 配置：

1. 选择 USART1 对应串口，设为 921600 baud、8N1、无流控。
2. 协议引擎选择 `FireWater`，程序会自动生成名为 `voltage` 的通道，单位为 V。
3. 每帧格式为 `voltage:1.234567\r\n`。不要在同一串口中混入其他调试日志。

测试流程：

1. 断电完成上表接线，核对 ADS8688 模块电源和参考电压要求，先不加输入信号。
2. 烧录 Debug 固件并复位，打开 VOFA+ 后观察 CH1 波形；CH1 接 0 V 时应接近 0 V，对应原始码约 `32768`。
3. 依次输入已知的直流正、负电压（例如 +1.000 V 和 -1.000 V），确认 VOFA+ 数值极性和比例正确。
4. 再输入 10 kHz、1 Vpp 正弦波，检查幅度和直流偏置。程序以 ADS8688 上限 500 kSPS 固定节拍采集1000点整帧，每帧采集完成后通过 FireWater 发送全部1000点，然后采集下一帧。10 kHz 每周期理论上约有50个点；VOFA+横轴是采样点序号，帧间会有串口发送空隙。
5. 若 VOFA+ 无数据，先用逻辑分析仪检查 `PB12/PB13/PB15`是否有 CS、SCK 和 SDI 时序，再检查 `PB14` 是否返回数据。初始化会读回 CH1 量程寄存，读回不匹配时主程序进入 `Error_Handler()` 且不发送 FireWater 帧。

## 淘晶驰 TJC1060X570_011C_I 串口屏

当前主程序已启用 USART3 和 G题串口屏应用。界面采用 `home → menu → control → keypad/result` 多级页面，支持要求2/3/4选择、计算器式频率/Vpp输入以及增益、源端幅度和ASF结果显示。

完整接线、控件名称、按钮事件和USART HMI配置见根目录 `USART.md`。代码入口为 `API/TJC_HMI_API.c`，底层协议位于 `HDL/TJC_HMI.c`。USART1仍连接电脑用于调试，不要把串口屏并联到USART1。

## 数字已知模型（PB1独立启动）

该功能不接入 USART3 串口屏。上电后处于停止状态，按下 PB1 才启动 ADC→IIR→DAC 连续处理，启动成功后 PC13 保持点亮。

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| 启动按键 | `PB1` | 内部上拉，按键接地，低电平有效 |
| 运行指示 | `PC13` | 默认按常见板载LED的低电平点亮；极性统一由 `main.h` 中两个 `G_MODEL_LED_*_STATE` 宏调整 |
| 模型输入 | `PA1_C / ADC1_INP1` | 0～3.3 V，建议以1.65 V为正弦偏置 |
| 模型输出 | `PA4 / DAC1_OUT1` | 0～3.3 V，软件中点1.65 V |

复制的目标传递函数为：

```text
              1
H(s) = -------------------
       1e-8*s^2 + 3e-4*s + 1
```

采样率为1 MS/s，用双线性变换获得CMSIS-DSP DF1二阶biquad系数。输入信号必须限制0～3.3 V并与开发板共地，不可直接输入负电压。

建议按以下顺序验收：

1. 复位后确认PC13灯灭，PA4尚未进入连续输出状态。
2. 按下PB1，确认PC13点亮；USB串口应打印 `MODEL ADC=PA1_C DAC=PA4 Fs=1000000 Hnum=1`。
3. PA1_C输入1.65 V直流，PA4应约为1.65 V。
4. 输入1 kHz、1 Vpp、偏置1.65 V正弦，理论输出约为0.5051 Vpp。
5. 100 Hz、500 Hz、2 kHz、3 kHz的理论幅度倍数依次约为0.9865、0.7668、0.2622、0.1612。
6. 示波器使用1 MΩ高阻输入，片上DAC不要直接驱动50 Ω负载。

# 项目状态

## 当前主任务：AD9959 四通道 100 kHz 正弦输出 + TIM2 测频（2026-07-28）

- `main.c` 当前初始化 GPIO、DMA、TIM2 和 USART1。AD9959 使用未启动 AD9910 的控制线：`PA8=SCLK`、`PD4=CS`、`PD5=IO_UPDATE`、`PA12=SDIO0`、`PA6=RESET`；以模块 DDS 系统时钟为 500 MHz 配置 CH0～CH3 同时输出 100 kHz、幅度码 512、相位码 0 的正弦。该幅度码只表示数字幅度约一半，实际 Vpp 仍受模块时钟、输出网络和负载影响。
- `PA0 / TIM2_CH1` 运行既有输入捕获测频：低频使用中断捕获，高频使用 DMA1 Stream5 的 `/8` 捕获；测量范围为 1 Hz～1 MHz，串口 `PB6/PB7` 以 921600 8N1 每秒输出一次 `freq=...` 摘要。当前主任务不启动 ADS8688、AD9910、AD9833、ADC、DAC、TIM1、TIM4、AD9226/DCMI、SPI 或 USART3。
- AD9959 和 TIM2 测频仅完成代码接入，尚未编译、烧录或上板验收。AD9959 模块数字逻辑若为 5 V，首次接线需确认 3.3 V 高电平门限或加入电平转换；PA0 只允许输入 0～3.3 V 方波且必须共地。

- 保留的 ADS8688 demo 仅初始化 GPIO、DMA 和 USART1，并调用 `ADS8688_API_Init()`；主循环仅调用 `ADS8688_API_Process()`。ADS8688 使用手动通道轮询读取模块 `CH1`～`CH4`（内部通道 `0`～`3`），CH5～CH8 关断掩码为 `0xF0`；四路均为 `+-10.24 V` 量程。
- ADS8688 使用 `PB13=SCLK`、`PB14=SDO`、`PB15=SDI` 的 GPIO 软件 SPI，`PB12` 仍为低有效片选。每轮依次对 CH1～CH4 发送 `MAN_Ch_n` 命令，再进行“CS 拉低、写两个 0x00、读高低字节”的转换读取。此模式规避了当前软件 SPI 下 AUTO_RST 自动扫描四路均读到 `0xFFFF` 的问题；当前为功能验收 demo，不承诺固定 100 kS/s/通道。
- USART1 保持 `PB6 / USART1_TX`、`PB7 / USART1_RX`、921600 8N1，使用 DMA1 Stream7 异步发送 CH1～CH4 的 FireWater 文本帧：`samples:<ch1>,<ch2>,<ch3>,<ch4>\r\n`。四个值依次对应模块 CH1、CH2、CH3、CH4；发送每 25 组四通道样本取一帧，串口忙时保留最新待发帧，不等待且不阻塞采集。
- 上电完成 USART1 初始化后，主程序先异步发送一行纯文本 `ok\r\n`，再初始化 ADS8688。`ok` 用于独立确认 USART1、DMA 与 PB6 接线；若看不到 `ok`，应先检查固件是否已烧录、USB-TTL RX→PB6、共地和 921600 8N1。`ok` 后仍无 ADS 四通道数据，则排查 ADS8688 初始化或 SPI 采集。
- ADC1/ADC2/ADC3、TIM1/TIM4/TIM6、片内 DAC、DAC8830、AD9833、AD9910、AD9226/DCMI、SPI1、USART3 和其他 demo 当前均不启动；AD9959 与 TIM2 测频已启动。

- 保留的片内 DAC 任务曾初始化 GPIO、DMA、DAC1 和 TIM4，并调用
  `DAC_Waveform_StartChannel(DAC_CHANNEL_1, DAC_USER_WAVE_SINE, 100000.0f, 1.0f, 1.65f)`。
  输出引脚为 `PA4 / DAC1_OUT1`，目标为 100 kHz、1 Vpp、1.65 V 偏置。片内 DAC 波形 API 的
  高频模式将活跃表缩短为 20 点，并将更新率设为 2 MS/s；TIM4 以 240 MHz 计数时钟、120 个
  时钟周期每点运行。尚需上板确认 DAC DMA 欠载、
  幅度和失真。DAC8830 直流驱动保留但当前不启动。
  输出需先核验高电平门限，首次实物调试建议加 3.3 V 到 5 V 电平转换。ADC2 FFT、DDS、
  片内 DAC、USART 和其他 demo 保留但不启动。

- 保留的 ADC2 FFT 任务初始化 GPIO、DMA、ADC2、TIM1 和 USART1；上电后运行一次 `ADC2_FFT_API_Start()`，主循环执行 `adc2_proc()` 与 `ADC2_FFT_API_Process()`。模拟输入为 `PA7 / ADC2_INP7`，必须共地并限制为 0～3.3 V。该任务先测频并选择最终采样率，再输出一次原始波形、半边频谱和最终频率；最终结果还输出 `wave=sine|triangle|square|unknown` 与 `h3`/`h5` 谐波比。AD9833 仍输出 1 kHz 正弦，AD9910 仍输出 100 kHz / 300 mVpp 正弦。TIM2 方波测频调用保留为注释，未启动。

- `FML/ADC2_CAPTURE_FML.*`、`BLL/ADC2_CAPTURE_BLL.*` 和 `API/ADC2_CAPTURE_API.*`按参考工程的 `MY_ADC2_Init()`、`adc2_deal()`、`adc2_proc()` 分层建立独立链路。ADC2 为 `PA7 / ADC2_INP7`；TIM1_TRGO 使用 240 MHz 计数时钟、PSC=0、ARR=239，目标/实际采样率均为 1 MS/s；ADC2 使用 10 bit、8.5 周期采样时间、无过采样。
- 新增可选 `API/SUPER_FFT.*`：ADC3 (`PC2_C / ADC3_INP0`) 通过 DMA1 Stream3 单帧采集 4096 点，并使用独立的 TIM6_TRGO，不再占用 TIM1。测频沿用参考工程“400 kS/s 高频粗测→40 kS/s 低频 FFT 粗测→1 Hz 步进细扫”的流程；TIM6 的 PSC/ARR 按实际 APB1 定时器时钟计算。DMA 缓冲区位于 RAM_D2、32 字节对齐，前后台维护 D-Cache，回调只置完成标志。该模块尚未接入 `main.c`，不得与 ADC3 的 VOFA 预设同时运行。`Core/Src/adc.c` 中 ADC3 的手工触发源改为 `ADC_EXTERNALTRIG_T6_TRGO`，与当前 `.ioc` 可能不一致，后续 CubeMX 重新生成前须恢复此配置。
- 本任务启动 GPIO、DMA、ADC2、TIM1 和 USART1。DMA1 Stream1 服务 ADC2，为 halfword、very-high、normal DMA，写入 `.dma_buffer` / RAM_D2 的 32 字节对齐 1024 点缓冲区。DMA 完成回调只置 `adc2_deal_flag`；前台 `adc2_proc()` 调用 `adc2_deal()`，失效 D-Cache、按 10 bit 换算 0～3.3 V、停止 ADC DMA，再以 USART1 `PB6/PB7`、921600 8N1 逐行发送 1024 个 `%.5f` 电压值。PA7 已配置为 analog 模式。进入 `while` 前还会启动 AD9833、AD9910 的 1 kHz 正弦输出。AD9910 将 PA8 配置为软件 SCK，故 TIM1_CH1 外部输出不可用，但 ADC2 使用的内部 TIM1_TRGO 不受影响。`ADC_VOFA_API_Process()` 的 ADC3 预设实现保留，但当前主任务不调用。ADC1、ADC3、DAC、TIM4、AD9226/DCMI、SPI 和 USART3 不启动。
- ADC 时序预算：1 MS/s 对应 80 个 ADC 时钟周期；10 bit 转换的采样 8.5 周期加转换周期低于该预算，因此满足当前采样率。实际前端阻抗仍可能要求更长采样时间。
- ADC2 的 PA7、独立 DMA1 Stream1 及 TIM1_TRGO 配置是当前 ADC端口预设的基础；尚未重新生成 CubeMX 代码，后续重新生成前必须在 `.ioc` 中复现 ADC2 的独立、TIM1_TRGO、normal DMA 设置。

更新时间：2026-07-27

## 已验证

- `cmake --build --preset Debug` 编译、链接通过。
- 当前 AD9226 demo 已编译、链接通过：
  - FLASH：70388 B / 128 KB（53.70%）。
  - DTCMRAM：20040 B / 128 KB（15.29%）。
  - RAM_D2：8704 B / 288 KB（2.95%）。
  - 尚未上板验证 DCMI 引脚、采样时钟、码序和 THD 结果。
- 当前串口屏 demo 构建后资源占用：
  - FLASH：41972 B / 128 KB（32.02%）。
  - DTCMRAM：3176 B / 128 KB（2.42%）。
  - RAM_D2：0 B / 288 KB。
- 淘晶驰串口屏 demo 已编译、链接通过，尚未下载 TFT 和固件进行实物联调。
- 上一次 AD9910 demo 构建后资源占用（串口屏 demo 构建值见后续验证）：
  - FLASH：42460 B / 128 KB（32.39%）。
  - DTCMRAM：3016 B / 128 KB（2.30%）。
  - RAM_D2：0 B / 288 KB。
- ADS8688 软件 SPI、CH1～CH4 手动通道轮询、电压换算和 VOFA+ FireWater 输出已于 2026-07-28 由用户上板确认可用；AUTO_RST 自动扫描路径曾出现四路 `0xFFFF`，当前不作为主任务使用。
- 双 ADC 相位差链路已编译、链接通过：ADC1/ADC2 同步采样、1 MS/s、4096 点 FFT、公共频率正弦拟合。
- 双ADC启动顺序已调整为“独立模式校准ADC1/ADC2→启用双重规则同步模式→启动公共CDR DMA”；采集增加100 ms超时寄存器诊断及DMA完成轮询兜底。
- CubeMX 测频配置已接入：PA0 / TIM2_CH1、32 位满量程计数、DMA1 Stream5、TIM2 和 DMA 中断优先级 2。
- 方波测频代码已编译、链接通过；1 Hz 到 1 MHz 范围和 0.5 Hz/10 Hz 输出分辨率仍需信号源实测确认。
- DAC8830 驱动已接入构建，新增文件：
  - `HDL/DAC8830.h`
  - `HDL/DAC8830.c`
  - `HDL/DAC8830_SPI.h`
  - `HDL/DAC8830_SPI.c`
- 二阶 IIR 低通与 ADC→DAC 主任务已编译、烧录并上板验证：`FML/IIR_FML.*`按双线性变换复刻`1 / (1e-8 s² + 3e-4 s + 1)`；`FML/IIR_ADDA_FML.*`使用 ADC1/DAC1 的 1024 点循环 DMA、同侧空闲半块回写和 D-Cache 维护，以 1 MS/s 运行。修复前版本已实测 1 kHz 交流增益接近理论值但出现严重可见失真；修复后 1 kHz PA4 原生 Vpp 重复读数标准差降至约 0.011 V，长窗波形导出仍受 USB 二进制块截断限制。

## 当前主程序行为
- 片内 DAC 波形任务及其串口参数命令代码均保留，但当前 DAC8830 主任务未启动片内 DAC、ADC 或 USART。DAC8830 的硬件 SPI 已迁至 `PB3/SPI1_SCK` 和 `PD7/SPI1_MOSI`，片选仍为 `PE2/PE0`，不再与 ADC2 的 PA7 或 DAC1_CH2 的 PA5 复用。当前 DAC8830 波形使用 TIM4 和 DMA1 Stream2/3/4，不与其他 TIM4 或这些 DMA 流的任务同时启动。

- 当前主任务运行独立 ADC2 采集链路：上电后启动 AD9833、AD9910 的 1 kHz 正弦，再挂接 ADC2 DMA 并启动 TIM1；采满 1024 点后，DMA 回调仅置标志，主循环调用 `adc2_proc()` 按参考工程流程换算并发送。`ADC_VOFA_API_Process()` 未被调用。已于 2026-07-25 编译、链接通过；尚未烧录或上板。
- 已提供可由应用在 `while` 前显式调用的 `void` 输出封装：`AD9833_API_OutputWaveform(frequency_hz, waveform)` 写 FREQ0/PHASE0/波形模式并释放 RESET；`AD9910_API_OutputSine(frequency_hz, amplitude_mvpp)` 直接包装 `AD9910_output_sine()`。当前 `main.c` 分别以 1 kHz 和 2 kHz 调用这两个输出封装。
- USART1已加入非阻塞运行时 PI 命令：仅`kp`、`ki`和只读`show`。每次命令仅回传一次`dpll`摘要；当前 PI 主任务关闭原`phase=...`周期诊断，Kp/Ki仅存于 RAM，复位恢复编译期默认值。

## 已实现模块

- `HDL/AD9226.*`：12 位 DCMI 并口、TIM1 采样时钟、同步门控和 DMA2 固定帧采集。
- `BLL/AD9226_BLL.*`：周期检测、单周期相干重采样、1024 点 FFT、基波至五次谐波和 THD。
- `API/AD9226_API.*`：当前验证 demo 初始化、逐帧处理、错误提示和 USART3 DMA 摘要输出。
- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码；`HDL/AD9910_Constants.h` 集中定义 ASF 上限与固定十倍后级的满量程标定参数。
- `HDL/AD9959.*`：参考 AD9959 GPIO 串行驱动移植的四通道 DDS 基础驱动；支持 CSR 通道选择、频率/幅度/相位寄存器和 IO_UPDATE 锁存。当前 `main.c` 用 PA8/PD4/PD5/PA12/PA6 启动四通道 100 kHz 正弦，尚未编译或上板验证。
- `HDL/ADS8688.*`：ADS8688 GPIO 软件 SPI 命令、程序寄存、量程、手动转换与自动扫描驱动。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移，底层使用 SPI1 硬件发送。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：双ADC同步DMA缓冲、校准、帧重启、回调状态和DCache维护。
- `FML/FFT_FML.*`：4096 点 FFT、窗函数、峰值和相位基础计算。
- `FML/FREQ_FML.*`：TIM2 输入捕获自动量程、低频中断、高频 DMA、DCache 维护、超时和频率结果；`API/FREQ_API.*` 的 USART1 摘要使用中断发送和单条最新值待发槽，不阻塞测频状态机。
- `FML/IIR_FML.*`：基于 CMSIS-DSP DF1 双二阶的单位增益二阶低通；系数随实际采样率初始化，状态跨数据块保持。
- `FML/IIR_ADDA_FML.*`：保留的 ADC1→IIR→DAC1 DMA 调度、ADC/DAC 定时器配置、DMA 缓冲区及 D-Cache 维护。
- `API/IIR_AD_DA_API.*`：IIR ADC→DAC 主任务启动入口。
- `API/DLIA_API.*`：当前数字锁相鉴相 demo 的双 ADC 帧调度、CDR 解包、相位摘要输出与重启入口。
- `BLL/FFT_BLL.*`：FFT 结果结构体和主峰/次峰插值频率。
- `BLL/PHASE_BLL.*`：公共频率细化、双通道正弦拟合、三角波谐波识别与亚采样互相关时延、相位差和校准补偿。
- `BLL/DLIA_BLL.*`：512 点共享 DDS 的双通道 IQ 鉴相器；`Tests/dlia_bll_selftest.c`提供合成 12 bit 数据的离线精度自检。
- `BLL/ADS8688_BLL.*`：ADS8688 16 bit 原始码到伏特值的量程换算。
- `API/ADC_API.*`：ADC 数据处理、FFT 计算和串口输出入口。
- `API/FFT_API.*`：独立 FFT 输出入口，当前主循环未直接调用。
- `API/FREQ_API.*`：测频初始化、主循环处理、结果访问和 USART1 摘要输出入口。
- `API/PHASE_API.*`：当前双ADC相位差demo入口、摘要输出和逐帧重启。
- `API/AD9910_API.*`：按波形类型、频率和直接输出 mVpp 参数启动 AD9910，支持幂等初始化与原始 14 位 ASF 单 Profile 正弦输出；连续正弦更新只重写 Profile 0，从 RAM 模式切回时才重新配置单 Profile 模式。
- `API/ADS8688_API.*`：CH1～CH4 手动通道轮询初始化、连续采样和四通道 VOFA+ FireWater 输出。
- `HDL/TJC_HMI.*`：淘晶驰字符串指令发送、文本/数值赋值、标准触摸帧和 demo 自定义帧解析。
- `API/TJC_HMI_API.*`：串口屏 demo 的页面初始化、周期刷新和按钮业务入口，当前主任务不调用。

## 当前引脚定义

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| USART1_TX | PB6 | 当前 ADC端口预设→VOFA+ 输出；与 AD9226 D5 复用 |
| USART1_RX | PB7 | 当前 ADC端口预设串口输入；与 AD9226 VSYNC 复用 |
| USART3_TX | PB10 | AD9226 摘要输出，115200 baud |
| USART3_RX | PB11 | 当前未使用 |
| AD9226_D0～D3 | PC6/PC7/PC8/PC9 | DCMI 12 位数据低四位 |
| AD9226_D4～D7 | PE4/PB6/PE5/PE6 | DCMI 12 位数据中四位 |
| AD9226_D8～D11 | PC10/PC12/PB5/PD2 | DCMI 12 位数据高四位 |
| AD9226_CLK | PA8 / TIM1_CH1 | 1 MHz 输出，同时回接 PA6 |
| DCMI_PIXCLK | PA6 | 接 PA8/AD9226 CLK 同一时钟网络 |
| DCMI_HSYNC_GATE | PB1 → PA4 | 必须外部短接 |
| DCMI_VSYNC_GATE | PB2 → PB7 | 必须外部短接 |
| ADS8688_CS | PB12 | ADS8688 片选，软件控制，低有效 |
| ADS8688_RST_PD | PC4 | ADS8688 复位/低功耗控制，demo 保持高电平 |
| ADS8688_SCK | PB13 | GPIO 软件 SPI 时钟 |
| ADS8688_SDO | PB14 | ADS8688 数据输出，GPIO 输入 |
| ADS8688_SDI | PB15 | ADS8688 命令输入，GPIO 输出 |
| FREQ_IN / TIM2_CH1 | PA0 | 方波测频输入，上升沿，0 到 3.3 V |
| ADC1_INP1 | PA1_C | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| ADC2_INP7 | PA7 | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| ADC3_INP0 | PC2_C | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| DAC1_OUT1 | PA4 | 片上 DAC1 CH1 输出 |
| DAC1_OUT2 | PA5 | 片上 DAC1 CH2 输出；与迁移后的 DAC8830 SPI1 引脚无复用 |
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
| AD9959_SCLK | PA8 | 当前 demo 软件串行时钟；与 AD9910 SCK、AD9226 TIM1_CH1 冲突 |
| AD9959_CS | PD4 | 当前 demo 软件串行片选；与 AD9910 CSN 冲突 |
| AD9959_IO_UPDATE | PD5 | 当前 demo 锁存脉冲；与 AD9910 IUP 冲突 |
| AD9959_SDIO0 | PA12 | 当前 demo 单向串行数据输出；与 AD9910 SDI 冲突 |
| AD9959_RESET | PA6 | 当前 demo 硬件复位；与 AD9910 MRT、AD9226 PIXCLK 回路冲突 |
| DAC8830_CS1 | PE2 | DAC8830 通道 A 片选 |
| DAC8830_CS2 | PE0 | DAC8830 通道 B 片选 |
| DAC8830_SDI | PD7 / SPI1_MOSI | DAC8830 硬件 SPI 数据 |
| DAC8830_SCLK | PB3 / SPI1_SCK | DAC8830 硬件 SPI 时钟 |

注意：本板封装的`AD9833_CS / FSYNC=PA1`与`ADC1_INP1=PA1_C`是独立焊盘，可同时使用；ADC 初始化须保持`SYSCFG_SWITCH_PA1`打开，使 PA1_C 直连 ADC，而 PA1 保持普通 GPIO。

## 待确认

- AD9226 目前仅完成编译验证。上板后需先确认 PA8/PA6 均为 1 MHz 时钟，再检查 12 根数据线位序、静态输入码、1 kHz 正弦的 `min/max/mean`，最后验收频率和 THD。
- IIR ADC→DAC 链路已完成 1 kHz 双通道实测：PA1_C 约 1.167 Vpp，PA4 约 0.603 Vpp，对应增益约 0.517，而理论值约 0.505。DMA 同侧空闲半块回写修复已烧录；修复后 PA4 的 9 次原生 Vpp 为 0.587～0.620 V，标准差约 0.011 V，频率约 999～1016 Hz，未再出现此前单次 1.133 Vpp 的明显离群读数。短窗 BYTE 波形导出成功但不用于幅值验收；覆盖完整 1.024 ms DMA 周期的长窗导出因 USB 二进制块截断失败。仍待复测 100 Hz、10 kHz、延迟、削顶、半块交界连续性及 ADC DMA 错误计数。
- AD9226 与 AD9910 共用 PA6/PA8，与片上 DAC 共用 PA4，与原板载 USART1 共用 PB6/PB7，并占用 TIM1；当前主任务不得同时初始化这些模块。切换回其他 demo 时需恢复对应 `.ioc` 引脚和 TIM1 配置。
- ADS8688 四通道手动轮询已由用户确认运行成功。后续建议逐路用已知直流电压复核零点、正负满量程、通道顺序和 VOFA+ 波形幅度。
- 当前 ADC端口预设将 `.ioc` 与生成的 ADC1 配置设为 halfword normal DMA/one-shot。PA1 和 PA1_C 是本板封装的独立焊盘：前者保留 AD9833-CS GPIO，后者为 ADC1_INP1 专用模拟直连输入。本次 DAC2 自检在生成代码中手工增加 PA5、DAC1_CH2 和 DMA1 Stream2，尚未回写 `.ioc`；后续生成代码前需在 CubeMX 中核对这些运行时覆盖项。
- 当前 ADS8688 demo 的 VOFA+ FireWater 使用四通道文本帧 `samples:<ch1>,<ch2>,<ch3>,<ch4>\r\n`；除上电 `ok` 心跳外，不得在同一 UART 插入其他文本日志，否则会产生无关通道或干扰解析。

- 方波测频尚未用实物信号源覆盖验证 1 Hz、频段切换点、10 kHz 分辨率切换点和 1 MHz 上限；应重点观察串口 `raw`、`mode`、`ticks` 和 `periods`。
- 当前 TIM2 标称时钟为 75 MHz，来自内部 HSI。已按本次 1 MHz 实测做单点比例校准，但系数可能随板卡、温度和时钟条件变化；高精度场景仍建议多频点复核或使用外部高精度时钟。
- PA0 输入必须限制在 0 到 3.3 V，并与信号源共地；5 V 方波不可直接输入。

- AD9226 的 TIM1_CH1 标称输出为 1 MHz；当前系统使用内部 HSI 派生时钟，实测前仍需用示波器复核 PA8/PA6 的真实频率和边沿质量。
- 相位校准表当前四个频点均为`0°`占位值。达到全频段0.5°指标前，必须使用同源一分二输入，在1 kHz、10 kHz、50 kHz、100 kHz测出通道固定相差并写入`BLL/PHASE_BLL.c`。
- 相干采样以第一帧频率估计为依据；若输入在两帧之间发生跳频或漂移，第二帧不再严格相干，需依靠`closure`、拟合质量和重复测量识别。
- `.ioc` 已按双 ADC 主链路补齐 ADC2 和 TIM1 配置；DAC8830 的 SPI1 已迁至 PB3/PD7，因此不再与 PA7 的 ADC2 输入互斥。
- 双ADC链路和算法尚未经过实物信号源验证；应覆盖1 kHz、100 kHz、接近±180°、不同幅度及低信噪比场景。
- 2026-07-17首次上板时仅看到启动日志、没有首帧结果；已针对ADC从机校准顺序进行修正。当前正常启动不再打印就绪信息；若采集超时，串口会输出`phase capture timeout ...`寄存器快照供继续定位。
- 2026-07-17新日志确认DMA每帧数据已完成，但当前中断配置下HAL DMA完成回调未进入；双ADC单帧采集现明确改为轮询`NDTR`完成，取消20 ms等待和`DMA callback missed`刷屏，仍保留100 ms真实超时诊断。后续已确认ADC2实际接在PA7并修正通道配置。
- 90°输入日志在相干采样率放宽到1～1.5 MS/s后曾出现大量随机离群，但同源分路测试接近0°。相位帧已收紧到1～1.1 MS/s、正弦拟合质量门限提高到0.98，并停止输出无效帧；后续正弦/三角波整体实物验收已通过。
- 三角波输入90°时，原基波正弦拟合实测仅输出72～75°。已新增自动波形分流：两路三次谐波比不低于0.04时，改用归一化互相关与黄金分割亚采样延迟估计；正弦仍使用原最小二乘拟合。合成理想三角波1～100 kHz、±90°验证中最大误差约0.051°，且用户已确认实物完整验收通过。
- 2026-07-17确认实际ADC2接线为PA7，原软件错配为PA2/ADC2_INP14，已更正为PA7/ADC2_INP7。DAC8830 的 SPI1_MOSI 已迁至 PD7，因此 ADC2 与 DAC8830 的数据引脚不再复用。
- 100 kHz下0.5°对应约13.9 ns；两路输入保护、偏置、RC和走线必须尽量一致，否则模拟链路相差会超过软件误差预算。
- AD9910 当前由主任务在 `while` 前完成幂等基础初始化，但不启动 DDS 波形。本次从 `h750_demo_project_2025g` 迁回幂等初始化、原始 ASF 正弦接口和统一标定常量；`AD9910_output_sine()` 的幅度语义为固定十倍后级的末级目标 Vpp，满量程为 `AD9910_OUTPUT_FULL_SCALE_MVPP`，因此在当前未接后级的 demo 接线中不应直接调用。既有正弦波、RAM 方波和 RAM 三角波实测记录仍适用于 `AD9910_API_StartWaveform()` 的直接输出语义；RAM 路径保持在50～1024点间搜索频率误差最小且点数最多的时序，配置为先禁用、装载后再使能，CFR1[16] 选择正弦输出，以90°/270°相位映射正负样本。新增 API 仅完成编译验证，尚未在本主分支上板复测。
- DAC8830 模块的量程换算已按 V1.1 使用手册修订：±10 V、±5 V、0~10 V 和 0~5 V 分别使用对应的 20 V、10 V、10 V 和 5 V 跨度；当前主任务使用 0~5 V 档。原正弦 demo 已由主程序启动。
- SPI1 初始化配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/4 分频；SCK 约 48 MHz，低于模块规定的 50 MHz 上限。
- DAC8830 高速正弦 TIM4/DMA 后端与其应用入口已移除。DAC8830 直流驱动保留但当前不启动；后续若恢复波形输出，应新建并单独完成 SPI 时序、DMA 与模拟滤波验收。

## 工作区注意

- 命令行编译/烧录统一入口为 `Tools/build.ps1` 和 `Tools/flash.ps1`；本机 CubeIDE VS Code / OpenOCD 绝对路径仅保存在被 Git 忽略的 `Tools/config/local.ps1`，可提交的模板为 `Tools/config/local.ps1.example`。烧录脚本固定先构建成功再下载，且校验 OpenOCD 的 programming、verify、reset 输出；除非用户明确授权，AI 不执行这两类脚本。
- 2026-07-23 已使用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\flash.ps1` 完成脚本实测：Debug 构建无待编译目标，CMSIS-DAPv2（序列号 `132765404740`）识别 STM32H7，OpenOCD 的 programming、verify、reset 均成功。脚本已兼容 OpenOCD stderr 横幅和 Windows 路径传给 OpenOCD Tcl 时的转义问题。
- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

## ADC2 FFT task update (2026-07-26)

- The active `main.c` task initializes GPIO, DMA, ADC2, TIM1, and USART1 only.
  It keeps AD9833 at 1 kHz sine and AD9910 at 100 kHz / 300 mVpp, but does not
  start the on-chip DAC, TIM4, or the DAC UART command task.
- ADC2 uses PA7 / ADC2_INP7, TIM1 TRGO, and DMA1 Stream1 in normal halfword mode.
  Two 4096-point frames are captured. The coarse frame is near 409.6 kS/s; its
  estimated 1-100 kHz frequency searches TIM1 rates from 360 to 409.6 kS/s
  for the final frame's lower cycle-closure error. This keeps FFT bin spacing
  no coarser than 100 Hz; divider 586 is 409556.31 S/s and 99.989 Hz.
- ADC2 capture and FFT are separated. `adc2_proc()` handles only capture polling,
  stop/cache handling, voltage conversion, and raw-frame publication. FFT
  accepts only raw samples, count, and sample rate in `ADC2_FFT_BLL`, using a
  table-free 4096-point radix-2 transform in `ADC2_FFT_FML` to fit the 128 KB
  FLASH region.
- After the final ADC2 FFT peak has been located, the API takes a third,
  classification-only frame at a TIM1 sample rate near an integer multiple of
  the measured frequency. `ADC2_FFT_BLL` compares the first six visible odd
  harmonic bands with sine, triangle, and square spectral profiles, reporting
  the best spectral score as `sine`, `triangle`, `square`, or `unknown`.
  Harmonic neighborhoods tolerate residual leakage. The fundamental is excluded
  from triangle/square spectral matching, so it cannot dominate the classifier;
  `h3`/`h5` remain diagnostic values only. Classification does not alter
  frequency estimation or coherent-capture selection.
- The classification frame is no longer capped at 1 MS/s. It may use up to
  2 MS/s and selects from 8 through 1024 samples per input period, preserving
  margin for ADC2, DMA, and analog-front-end timing while keeping more odd
  harmonics below Nyquist for high-frequency triangle-wave classification.
  ADC2 capture errors now stop TIM1, propagate to the API, and print an
  `adc2fft error stage=...` diagnostic instead of leaving the task silent. Each
  capture stage prints a short marker and has a 100 ms front-end timeout.
- USART1 is 921600 8N1. After the final frame only, UART sends `raw begin`, 4096
  `raw:<10-bit-code>` lines, `raw end`, then the 2048-bin nonredundant spectrum
  as `spectrum begin`, `fft:<bin>,<amplitude-v>`, and `spectrum end`, followed by
  one `result` line. The coarse frame is not transmitted. The blocking transfer
  is intentionally after sampling is stopped and takes a visible UART interval.
- `Tools/build.ps1` completed successfully on 2026-07-26: FLASH 76532 B / 128 KB
  (58.39%), DTCMRAM 64456 B / 128 KB (49.18%), RAM_D2 12 KB / 288 KB (4.17%).
  This task has not been flashed or verified on hardware. Keep PA7 within 0-3.3 V,
  share ground, and disconnect AD9226 wiring from PB6/PB7 before USART1 use.
## Generic FFT BLL input (2026-07-26)

- `ADC2_FFT_BLL_Analyze()` now receives `adc2_fft_input_config_t`, not fixed
  ADC2 conversion constants. The descriptor contains real sample rate, 4096
  sample count, 1-16 bit width, raw encoding, volts per LSB, and peak search
  bounds. It supports unsigned, offset-binary, and two's-complement `uint16_t`
  data from any independent ADC capture chain.
- The current ADC2 API supplies its existing 10-bit unsigned, `3.3/1023 V/LSB`,
  and 1-100 kHz configuration. Its acquisition task remains unchanged. FFT is
  still fixed to a 4096-point transform. Peak search includes bins straddling
  both configured limits, preventing actual 1 kHz input from being excluded
  when the integer TIM1 divider makes its bin position slightly above 10.
## ADC2 FFT single-shot update (2026-07-26)

- The ADC2 FFT API remains in `DONE` after one final-frame report. It performs
  one coarse capture and one coherent final capture after reset, sends raw data,
  half-spectrum, and frequency once, then keeps ADC2 sampling stopped.
## SUPER_FFT main task update (2026-07-26)

- `main.c` now starts `SUPER_FFT` on ADC3 (`PC2_C / ADC3_INP0`). ADC2 FFT calls,
  ADC2 initialization, and TIM1 initialization are retained as comments and are
  not active. ADC3 is triggered by internal TIM6 TRGO and uses DMA1 Stream3.
- The current `SUPER_FFT` module measures internally but does not format or send
  an internal UART result. `main.c` initializes USART1, restarts `SUPER_FFT`
  after every completed result, and sends `freq=<measured-Hz>Hz` only on the
  first result or when the measured frequency rounded to 1 Hz changes. The
  result remains available through `SUPER_FFT_GetFrequencyHz()`.
- USART1 result output uses interrupt-driven `HAL_UART_Transmit_IT()` with a
  short static buffer and one coalescing pending value. The FFT loop never waits
  for UART completion; when USART1 is busy, only the newest changed frequency is
  retained for the next transmit.
- `SUPER_FFT` no longer calls the legacy CMSIS 4096-point CFFT path. Its coarse
  stages use `SUPER_FFT_FML` windowed direct DFT power for only the searched bins;
  the 4096-point window, bin search, power accumulation, interpolation, and
  low-frequency fine correlation remain unchanged. This trades update speed for
  removing the CMSIS 4096-point twiddle and bit-reversal tables from FLASH.
- The 400 kS/s first-stage search covers 10 Hz to 100 kHz before deciding whether
  to hand off below 12 kHz to the 40 kS/s low-frequency fine scan. The low stage
  now covers 10 Hz to 12 kHz, so an actual 10 kHz signal remains eligible for
  the 1 Hz fine scan even when high-rate coarse FFT interpolation lands slightly
  above the 10 kHz boundary. This fixes the former 10 kHz lower search boundary,
  which could classify a 10 Hz signal as an arbitrary high-frequency noise peak.

# 项目状态

## 当前主任务：DDS 与片内 DAC 同时输出（2026-07-26）

- `FML/ADC2_CAPTURE_FML.*`、`BLL/ADC2_CAPTURE_BLL.*` 和 `API/ADC2_CAPTURE_API.*`按参考工程的 `MY_ADC2_Init()`、`adc2_deal()`、`adc2_proc()` 分层建立独立链路。ADC2 为 `PA7 / ADC2_INP7`；TIM1_TRGO 使用 240 MHz 计数时钟、PSC=0、ARR=239，目标/实际采样率均为 1 MS/s；ADC2 使用 10 bit、8.5 周期采样时间、无过采样。
- 新增可选 `API/SUPER_FFT.*`：ADC3 (`PC2_C / ADC3_INP0`) 通过 DMA1 Stream3 单帧采集 4096 点，并使用独立的 TIM6_TRGO，不再占用 TIM1。测频沿用参考工程“400 kS/s 高频粗测→40 kS/s 低频 FFT 粗测→1 Hz 步进细扫”的流程；TIM6 的 PSC/ARR 按实际 APB1 定时器时钟计算。DMA 缓冲区位于 RAM_D2、32 字节对齐，前后台维护 D-Cache，回调只置完成标志。该模块尚未接入 `main.c`，不得与 ADC3 的 VOFA 预设同时运行。`Core/Src/adc.c` 中 ADC3 的手工触发源改为 `ADC_EXTERNALTRIG_T6_TRGO`，与当前 `.ioc` 可能不一致，后续 CubeMX 重新生成前须恢复此配置。
- 本任务启动 GPIO、DMA、ADC2、TIM1 和 USART1。DMA1 Stream1 服务 ADC2，为 halfword、very-high、normal DMA，写入 `.dma_buffer` / RAM_D2 的 32 字节对齐 1024 点缓冲区。DMA 完成回调只置 `adc2_deal_flag`；前台 `adc2_proc()` 调用 `adc2_deal()`，失效 D-Cache、按 10 bit 换算 0～3.3 V、停止 ADC DMA，再以 USART1 `PB6/PB7`、921600 8N1 逐行发送 1024 个 `%.5f` 电压值。PA7 已配置为 analog 模式。进入 `while` 前还会启动 AD9833、AD9910 的 1 kHz 正弦输出。AD9910 将 PA8 配置为软件 SCK，故 TIM1_CH1 外部输出不可用，但 ADC2 使用的内部 TIM1_TRGO 不受影响。`ADC_VOFA_API_Process()` 的 ADC3 预设实现保留，但当前主任务不调用。ADC1、ADC3、DAC、TIM4、AD9226/DCMI、SPI 和 USART3 不启动。
- ADC 时序预算：1 MS/s 对应 80 个 ADC 时钟周期；10 bit 转换的采样 8.5 周期加转换周期低于该预算，因此满足当前采样率。实际前端阻抗仍可能要求更长采样时间。
- ADC2 的 PA7、独立 DMA1 Stream1 及 TIM1_TRGO 配置是当前 ADC端口预设的基础；尚未重新生成 CubeMX 代码，后续重新生成前必须在 `.ioc` 中复现 ADC2 的独立、TIM1_TRGO、normal DMA 设置。

更新时间：2026-07-24

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
- ADS8688 硬件 SPI 驱动、CH1 手动采样、电压换算和 VOFA+ FireWater 输出已编译、链接通过，尚未上板验证。
- 双 ADC 相位差链路已编译、链接通过：ADC1/ADC2 同步采样、1 MS/s、4096 点 FFT、公共频率正弦拟合。
- 双ADC启动顺序已调整为“独立模式校准ADC1/ADC2→启用双重规则同步模式→启动公共CDR DMA”；采集增加100 ms超时寄存器诊断及DMA完成轮询兜底。
- CubeMX 测频配置已接入：PA0 / TIM2_CH1、32 位满量程计数、DMA1 Stream5、TIM2 和 DMA 中断优先级 2。
- 方波测频代码已编译、链接通过；1 Hz 到 1 MHz 范围和 0.5 Hz/10 Hz 输出分辨率仍需信号源实测确认。
- DAC8830 驱动已接入构建，新增文件：
  - `HDL/DAC8830.h`
  - `HDL/DAC8830.c`
  - `HDL/DAC8830_DMA.h`
  - `HDL/DAC8830_DMA.c`
  - `HDL/DAC8830_SPI.h`
  - `HDL/DAC8830_SPI.c`
- 二阶 IIR 低通与 ADC→DAC 主任务已编译、烧录并上板验证：`FML/IIR_FML.*`按双线性变换复刻`1 / (1e-8 s² + 3e-4 s + 1)`；`FML/IIR_ADDA_FML.*`使用 ADC1/DAC1 的 1024 点循环 DMA、同侧空闲半块回写和 D-Cache 维护，以 1 MS/s 运行。修复前版本已实测 1 kHz 交流增益接近理论值但出现严重可见失真；修复后 1 kHz PA4 原生 Vpp 重复读数标准差降至约 0.011 V，长窗波形导出仍受 USB 二进制块截断限制。

## 当前主程序行为
- 当前主任务同时启动 AD9833 的 1 kHz 正弦、AD9910 的 100 kHz、500 mVpp 正弦和片内 `DAC1_CH1 / PA4` 的 1 kHz、1 Vpp、1.65 V 偏置方波。DAC 使用 `TIM4_TRGO` 触发、DMA1 Stream6 的普通单缓冲循环 DMA 搬运 256 点波表；正弦、方波、三角波和直流各有一张独立 DMA 源表。USART1 已在 PB6/PB7、921600 8N1 启动片内 DAC 参数命令，逐行发送 `波形编号 频率Hz Vpp 偏置V`，编号为 0 正弦、1 方波、2 三角波、3 直流；有效命令由前台先 stop 再 start 片内 DAC，返回 `ok`，格式或参数错误返回 `err`，DDS 输出不受影响。直接在运行中调用原有 `DAC_Waveform_StartChannel()`、`Start()` 或 `Apply()` 仍返回 `HAL_BUSY`，须先调用 `DAC_Waveform_Stop()` 后再以新参数启动，避免改写 DMA 正在读取的表。DMA 或 DAC 欠载错误后返回 `HAL_ERROR`，同样需 stop 后重新启动。IIR ADC→DAC 链路保留原有的同侧半缓冲回写调度，未改为此处的单缓冲模式。`TIM6_DAC_IRQn` 已接入 HAL 的欠载处理。AD9833 使用 PA1/PH4/PH5；AD9910 使用 PA6、PA8、PA12、PD4、PD5 和 PG7/PG9/PG11，与 PA4、TIM4、DMA1 Stream6、USART1 的路径不冲突。`PA5 / DAC1_CH2` 仍可通过原 API 选择，但它与 DAC8830 的 `SPI1_SCK` 复用，首次验收不使用该引脚。该组合已编译、链接通过，尚未烧录或上板验证。

- 当前主任务运行独立 ADC2 采集链路：上电后启动 AD9833、AD9910 的 1 kHz 正弦，再挂接 ADC2 DMA 并启动 TIM1；采满 1024 点后，DMA 回调仅置标志，主循环调用 `adc2_proc()` 按参考工程流程换算并发送。`ADC_VOFA_API_Process()` 未被调用。已于 2026-07-25 编译、链接通过；尚未烧录或上板。
- 已提供可由应用在 `while` 前显式调用的 `void` 输出封装：`AD9833_API_OutputWaveform(frequency_hz, waveform)` 写 FREQ0/PHASE0/波形模式并释放 RESET；`AD9910_API_OutputSine(frequency_hz, amplitude_mvpp)` 直接包装 `AD9910_output_sine()`。当前 `main.c` 分别以 1 kHz 和 2 kHz 调用这两个输出封装。
- USART1已加入非阻塞运行时 PI 命令：仅`kp`、`ki`和只读`show`。每次命令仅回传一次`dpll`摘要；当前 PI 主任务关闭原`phase=...`周期诊断，Kp/Ki仅存于 RAM，复位恢复编译期默认值。

## 已实现模块

- `HDL/AD9226.*`：12 位 DCMI 并口、TIM1 采样时钟、同步门控和 DMA2 固定帧采集。
- `BLL/AD9226_BLL.*`：周期检测、单周期相干重采样、1024 点 FFT、基波至五次谐波和 THD。
- `API/AD9226_API.*`：当前验证 demo 初始化、逐帧处理、错误提示和 USART3 DMA 摘要输出。
- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码；`HDL/AD9910_Constants.h` 集中定义 ASF 上限与固定十倍后级的满量程标定参数。
- `HDL/ADS8688.*`：ADS8688 SPI2 命令、程序寄存、量程和手动转换驱动。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移，底层使用 SPI1 硬件发送。
- `HDL/DAC8830_DMA.*`：TIM4 + DMA 波形输出，DMA1 Stream2 拉低 CS、Stream3 写 SPI1 TXDR、Stream4 拉高 CS。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：双ADC同步DMA缓冲、校准、帧重启、回调状态和DCache维护。
- `FML/FFT_FML.*`：4096 点 FFT、窗函数、峰值和相位基础计算。
- `FML/FREQ_FML.*`：TIM2 输入捕获自动量程、低频中断、高频 DMA、DCache 维护、超时和频率结果。
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
- `API/ADS8688_API.*`：CH1 demo 初始化、连续采样和 VOFA+ FireWater 输出。
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
| ADS8688_SCK | PB13 / SPI2_SCK | 硬件 SPI 时钟，17 MHz |
| ADS8688_SDO | PB14 / SPI2_MISO | ADS8688 数据输出 |
| ADS8688_SDI | PB15 / SPI2_MOSI | ADS8688 命令输入 |
| FREQ_IN / TIM2_CH1 | PA0 | 方波测频输入，上升沿，0 到 3.3 V |
| ADC1_INP1 | PA1_C | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| ADC2_INP7 | PA7 | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| ADC3_INP0 | PC2_C | 当前 ADC端口预设采样输入，必须限制在 0～3.3 V |
| DAC1_OUT1 | PA4 | 片上 DAC1 CH1 输出 |
| DAC1_OUT2 | PA5 | 片上 DAC1 CH2 输出；当前 DAC2 自检使用，与 DAC8830 的 SPI1_SCK 复用 |
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
| DAC8830_CS1 | PE2 | DAC8830 通道 A 片选 |
| DAC8830_CS2 | PE0 | DAC8830 通道 B 片选 |
| DAC8830_SDI | PA7 / SPI1_MOSI | DAC8830 硬件 SPI 数据 |
| DAC8830_SCLK | PA5 / SPI1_SCK | DAC8830 硬件 SPI 时钟 |

注意：本板封装的`AD9833_CS / FSYNC=PA1`与`ADC1_INP1=PA1_C`是独立焊盘，可同时使用；ADC 初始化须保持`SYSCFG_SWITCH_PA1`打开，使 PA1_C 直连 ADC，而 PA1 保持普通 GPIO。

## 待确认

- AD9226 目前仅完成编译验证。上板后需先确认 PA8/PA6 均为 1 MHz 时钟，再检查 12 根数据线位序、静态输入码、1 kHz 正弦的 `min/max/mean`，最后验收频率和 THD。
- IIR ADC→DAC 链路已完成 1 kHz 双通道实测：PA1_C 约 1.167 Vpp，PA4 约 0.603 Vpp，对应增益约 0.517，而理论值约 0.505。DMA 同侧空闲半块回写修复已烧录；修复后 PA4 的 9 次原生 Vpp 为 0.587～0.620 V，标准差约 0.011 V，频率约 999～1016 Hz，未再出现此前单次 1.133 Vpp 的明显离群读数。短窗 BYTE 波形导出成功但不用于幅值验收；覆盖完整 1.024 ms DMA 周期的长窗导出因 USB 二进制块截断失败。仍待复测 100 Hz、10 kHz、延迟、削顶、半块交界连续性及 ADC DMA 错误计数。
- AD9226 与 AD9910 共用 PA6/PA8，与片上 DAC 共用 PA4，与原板载 USART1 共用 PB6/PB7，并占用 TIM1；当前主任务不得同时初始化这些模块。切换回其他 demo 时需恢复对应 `.ioc` 引脚和 TIM1 配置。
- ADS8688 demo 目前只是“已编译”，需要用 CH1 实际电压完成通信、零点、正负满量程和 VOFA+ 波形验收。
- 当前 ADC端口预设将 `.ioc` 与生成的 ADC1 配置设为 halfword normal DMA/one-shot。PA1 和 PA1_C 是本板封装的独立焊盘：前者保留 AD9833-CS GPIO，后者为 ADC1_INP1 专用模拟直连输入。本次 DAC2 自检在生成代码中手工增加 PA5、DAC1_CH2 和 DMA1 Stream2，尚未回写 `.ioc`；后续生成代码前需在 CubeMX 中核对这些运行时覆盖项。
- VOFA+ FireWater 依赖换行分帧，不应在同一 UART 中插入其他日志，否则会增加无关通道或造成解析干扰。

- 方波测频尚未用实物信号源覆盖验证 1 Hz、频段切换点、10 kHz 分辨率切换点和 1 MHz 上限；应重点观察串口 `raw`、`mode`、`ticks` 和 `periods`。
- 当前 TIM2 标称时钟为 75 MHz，来自内部 HSI。已按本次 1 MHz 实测做单点比例校准，但系数可能随板卡、温度和时钟条件变化；高精度场景仍建议多频点复核或使用外部高精度时钟。
- PA0 输入必须限制在 0 到 3.3 V，并与信号源共地；5 V 方波不可直接输入。

- AD9226 的 TIM1_CH1 标称输出为 1 MHz；当前系统使用内部 HSI 派生时钟，实测前仍需用示波器复核 PA8/PA6 的真实频率和边沿质量。
- 相位校准表当前四个频点均为`0°`占位值。达到全频段0.5°指标前，必须使用同源一分二输入，在1 kHz、10 kHz、50 kHz、100 kHz测出通道固定相差并写入`BLL/PHASE_BLL.c`。
- 相干采样以第一帧频率估计为依据；若输入在两帧之间发生跳频或漂移，第二帧不再严格相干，需依靠`closure`、拟合质量和重复测量识别。
- `.ioc` 已按双 ADC 主链路补齐 ADC2 和 TIM1 配置，但 PA7 的 ADC2/SPI1_MOSI 互斥仍需在切换相位与 DAC8830 任务时人工确认；不要在同一主任务中同时初始化二者。
- 双ADC链路和算法尚未经过实物信号源验证；应覆盖1 kHz、100 kHz、接近±180°、不同幅度及低信噪比场景。
- 2026-07-17首次上板时仅看到启动日志、没有首帧结果；已针对ADC从机校准顺序进行修正。当前正常启动不再打印就绪信息；若采集超时，串口会输出`phase capture timeout ...`寄存器快照供继续定位。
- 2026-07-17新日志确认DMA每帧数据已完成，但当前中断配置下HAL DMA完成回调未进入；双ADC单帧采集现明确改为轮询`NDTR`完成，取消20 ms等待和`DMA callback missed`刷屏，仍保留100 ms真实超时诊断。后续已确认ADC2实际接在PA7并修正通道配置。
- 90°输入日志在相干采样率放宽到1～1.5 MS/s后曾出现大量随机离群，但同源分路测试接近0°。相位帧已收紧到1～1.1 MS/s、正弦拟合质量门限提高到0.98，并停止输出无效帧；后续正弦/三角波整体实物验收已通过。
- 三角波输入90°时，原基波正弦拟合实测仅输出72～75°。已新增自动波形分流：两路三次谐波比不低于0.04时，改用归一化互相关与黄金分割亚采样延迟估计；正弦仍使用原最小二乘拟合。合成理想三角波1～100 kHz、±90°验证中最大误差约0.051°，且用户已确认实物完整验收通过。
- 2026-07-17确认实际ADC2接线为PA7，原软件错配为PA2/ADC2_INP14，已更正为PA7/ADC2_INP7。PA7同时是DAC8830预留的SPI1_MOSI，当前相位demo不初始化SPI1/DAC8830；两项功能不可同时使用。
- 100 kHz下0.5°对应约13.9 ns；两路输入保护、偏置、RC和走线必须尽量一致，否则模拟链路相差会超过软件误差预算。
- AD9910 当前由主任务在 `while` 前完成幂等基础初始化，但不启动 DDS 波形。本次从 `h750_demo_project_2025g` 迁回幂等初始化、原始 ASF 正弦接口和统一标定常量；`AD9910_output_sine()` 的幅度语义为固定十倍后级的末级目标 Vpp，满量程为 `AD9910_OUTPUT_FULL_SCALE_MVPP`，因此在当前未接后级的 demo 接线中不应直接调用。既有正弦波、RAM 方波和 RAM 三角波实测记录仍适用于 `AD9910_API_StartWaveform()` 的直接输出语义；RAM 路径保持在50～1024点间搜索频率误差最小且点数最多的时序，配置为先禁用、装载后再使能，CFR1[16] 选择正弦输出，以90°/270°相位映射正负样本。新增 API 仅完成编译验证，尚未在本主分支上板复测。
- DAC8830 模块的软件换算固定为 ±10 V，原正弦 demo 参数为 10 kHz、1 Vpp、250 点；该 demo 当前不由主程序启动。
- SPI1 初始化已配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/2 分频；当前 SPI123 时钟改用 PLL3，目标 120 MHz，SCK 约 60 Mbit/s。
- DAC8830 正弦模块使用 250 点表和 TIM4 DMA 定时，目标更新率 2.5 MS/s；该链路当前不运行，重新启用后仍需用示波器或逻辑分析仪确认 10 kHz 输出和阶梯/毛刺情况。

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
  estimated 10-100 kHz frequency searches TIM1 rates from 360 to 409.6 kS/s
  for the final frame's lower cycle-closure error. This keeps FFT bin spacing
  no coarser than 100 Hz; divider 586 is 409556.31 S/s and 99.989 Hz.
- ADC2 capture and FFT are separated. `adc2_proc()` handles only capture polling,
  stop/cache handling, voltage conversion, and raw-frame publication. FFT
  accepts only raw samples, count, and sample rate in `ADC2_FFT_BLL`, using a
  table-free 4096-point radix-2 transform in `ADC2_FFT_FML` to fit the 128 KB
  FLASH region.
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
  and 10-100 kHz configuration. Its acquisition task remains unchanged. FFT is
  still fixed to a 4096-point transform.
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
- The 400 kS/s first-stage search covers 10 Hz to 100 kHz before deciding whether
  to hand off below 12 kHz to the 40 kS/s low-frequency fine scan. The low stage
  now covers 10 Hz to 12 kHz, so an actual 10 kHz signal remains eligible for
  the 1 Hz fine scan even when high-rate coarse FFT interpolation lands slightly
  above the 10 kHz boundary. This fixes the former 10 kHz lower search boundary,
  which could classify a 10 Hz signal as an arbitrary high-frequency noise peak.

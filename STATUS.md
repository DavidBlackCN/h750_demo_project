# 项目状态

## 当前主任务：AD9833 数字 PI 锁相 demo（2026-07-24）

- `API/AD9833_API.*`在启动时写入 AD9833 的 FREQ0=10 kHz、PHASE0=0°和正弦输出模式，再释放 RESET；板载 AD9833 的 FCLK 常量为 25 MHz，后续相位更新只写 PHASE0。
- `API/DLIA_API.*`以约1 MS/s同步采样 ADC1 `PA1_C / ADC1_INP1`（信号发生器/示波器CH2）和 ADC2 `PA7 / ADC2_INP7`（AD9833/示波器CH1），原始相位为`ADC2-ADC1=CH1-CH2`。`API/DPLL_API.*`在进入 PI 前取反为示波器标准`CH2-CH1`，每10 ms在前台写 AD9833 PHASE0；默认目标0°、Kp=0.10、Ki=0.050、相位校准0°，初始相位命令180°。积分相位按360°环绕而非钳位，以补偿独立时钟频差。
- 本任务启动 GPIO、DMA、ADC1、ADC2、TIM1、USART1和 AD9833 软件 SPI；PA1 是 AD9833 FSYNC/CS，PH4 是 SDATA，PH5 是 SCLK。DAC、TIM4、AD9226/DCMI和 SPI 外设不启动。AD9833输出送入 ADC2 前仍须经跟随、1.65 V偏置和0~3.3 V限幅。

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

- 当前主任务运行 AD9833 数字 PI 锁相 demo。启动 AD9833 FREQ0=10 kHz、PHASE0=0°后，`DLIA_API`以双ADC normal DMA测量`ADC2-ADC1=CH1-CH2`，`DPLL_API`取反为`CH2-CH1`并每10 ms使用滤波相位更新 PHASE0。积分相位按360°环绕；PI 更新不在 DMA 回调执行。已于2026-07-24编译、烧录，闭环复验待重新测量。
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
| USART1_TX | PB6 | 当前 ADC→VOFA+ 任务输出；与 AD9226 D5 复用 |
| USART1_RX | PB7 | 当前 ADC→VOFA+ 任务输入；与 AD9226 VSYNC 复用 |
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
| ADC1_INP1 | PA1_C | ADC1 采样输入 |
| ADC2_INP7 | PA7 | ADC2 采样输入；相位demo中使用 |
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
- `.ioc` 的 ADC1 初始 DMA 仍是半字循环模式，但当前鉴相 demo 在运行时明确覆盖为双 ADC 公共 CDR 的字宽 normal DMA。PA1 和 PA1_C 是本板封装的独立焊盘：前者保留 AD9833-CS GPIO，后者为 ADC1_INP1 专用模拟直连输入。本次 DAC2 自检在生成代码中手工增加 PA5、DAC1_CH2 和 DMA1 Stream2，尚未回写 `.ioc`；后续生成代码前需在 CubeMX 中核对这些运行时覆盖项。
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
- AD9910 当前不由主任务启动。本次从 `h750_demo_project_2025g` 迁回幂等初始化、原始 ASF 正弦接口和统一标定常量；`AD9910_output_sine()` 的幅度语义为固定十倍后级的末级目标 Vpp，满量程为 `AD9910_OUTPUT_FULL_SCALE_MVPP`，因此在当前未接后级的 demo 接线中不应直接调用。既有正弦波、RAM 方波和 RAM 三角波实测记录仍适用于 `AD9910_API_StartWaveform()` 的直接输出语义；RAM 路径保持在50～1024点间搜索频率误差最小且点数最多的时序，配置为先禁用、装载后再使能，CFR1[16] 选择正弦输出，以90°/270°相位映射正负样本。新增 API 仅完成编译验证，尚未在本主分支上板复测。
- DAC8830 模块的软件换算固定为 ±10 V，原正弦 demo 参数为 10 kHz、1 Vpp、250 点；该 demo 当前不由主程序启动。
- SPI1 初始化已配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/2 分频；当前 SPI123 时钟改用 PLL3，目标 120 MHz，SCK 约 60 Mbit/s。
- DAC8830 正弦模块使用 250 点表和 TIM4 DMA 定时，目标更新率 2.5 MS/s；该链路当前不运行，重新启用后仍需用示波器或逻辑分析仪确认 10 kHz 输出和阶梯/毛刺情况。

## 工作区注意

- 命令行编译/烧录统一入口为 `Tools/build.ps1` 和 `Tools/flash.ps1`；本机 CubeIDE VS Code / OpenOCD 绝对路径仅保存在被 Git 忽略的 `Tools/config/local.ps1`，可提交的模板为 `Tools/config/local.ps1.example`。烧录脚本固定先构建成功再下载，且校验 OpenOCD 的 programming、verify、reset 输出；除非用户明确授权，AI 不执行这两类脚本。
- 2026-07-23 已使用 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\flash.ps1` 完成脚本实测：Debug 构建无待编译目标，CMSIS-DAPv2（序列号 `132765404740`）识别 STM32H7，OpenOCD 的 programming、verify、reset 均成功。脚本已兼容 OpenOCD stderr 横幅和 Windows 路径传给 OpenOCD Tcl 时的转义问题。
- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

# 项目状态

更新时间：2026-07-20

## 已验证

- G 题基本要求 USART1 版本已在独立 `build/CodexVerify` 目录完成 Debug 编译、链接：
  - FLASH：41820 B / 128 KB（31.91%）。
  - DTCMRAM：3016 B / 128 KB（2.30%）。
  - RAM_D2：0 B / 288 KB。
- 30点增益数组以题给传递函数理论值为基线，软件自检遍历全部30×11参数组合。当前实测校准值：500 Hz根据1.2 V目标实测1.13 V调为3.4901266；1 kHz经1.96 V和2.09 V反馈调为2.3089042；2 kHz根据1.8 V目标实测1.72 V调为1.1953169。由于实测校准后相邻点可能局部非严格单调，自检改为验证增益正值/合理范围和全参数可计算性。
- `build/Debug` 中绑定工程改名前旧目录的缓存已删除并重新生成；当前 `CMAKE_HOME_DIRECTORY` 和工具链路径均指向 `h750_demo_project_2025g`，VS Code Debug 配置与构建已恢复。
- 当前 G题+串口屏构建后资源占用：
  - FLASH：78272 B / 128 KB（59.72%）。
  - DTCMRAM：4720 B / 128 KB（3.60%）。
  - RAM_D2：2 KB / 288 KB（0.69%）。
- 数字已知模型已编译、链接通过：1 MS/s ADC1 循环 DMA、CMSIS-DSP 二阶 IIR、DAC1_CH1 循环 DMA；100 Hz～3 kHz 离散模型相对目标连续模型的理论幅频误差最大约 0.004%，尚未上板验证。
- 淘晶驰多级菜单、共用数字键盘、要求2/3/4控制和结果页已编译、链接通过，尚未下载 TFT 进行实物联调。
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

## 当前主程序行为

- 当前主任务为完整G题应用；上电不运行AD9910要求2/3/4，数字已知模型也保持停止。PB1按下后独立启动数字模型并点亮PC13；该功能不接入USART3串口屏。
- `Core/Src/main.c` 同时初始化 `G_ConsoleAPI` 和 `TJC_HMI_API`，主循环并行处理USART1调试命令与USART3串口屏事件。
- CubeMX 本次生成后 CPU 主频为 480 MHz、AXI/AHB 为 240 MHz，`main.c` 显式开启 I/D Cache。
- USART1 使用 PB6/PB7、921600 8N1，承担参数输入和诊断日志输出；USART3使用PB10/PB11、115200 8N1，连接淘晶驰串口屏。
- PB1为数字已知模型独立启动按键（内部上拉、低有效）；PC13为运行LED，默认低电平点亮。这两个手工GPIO配置尚未回填 `adc_fft_demo.ioc`，重新生成CubeMX代码时需先同步。
- 已修正 `Core/Src/uart_redirect.c` 未加入 CMake 导致 `printf` 不会真正发送到 USART1 的问题；当前正常日志精简为频率、AD9910 直接输出 Vpp、10 倍后级输出 Vpp、ASF 和对应模型增益，详细诊断只在错误时输出。

## 已实现模块

- `HDL/AD9833.*`：AD9833 寄存器写入、频率、相位和波形控制。
- `HDL/AD9910.*`：AD9910 GPIO、寄存器写入和 Profile 设置基础代码。
- `HDL/ADS8688.*`：ADS8688 SPI2 命令、程序寄存、量程和手动转换驱动。
- `HDL/DAC8830.*`：DAC8830 双通道写码值、毫伏输出、量程选择、零码校准偏移，底层使用 SPI1 硬件发送。
- `HDL/DAC8830_DMA.*`：TIM4 + DMA 波形输出，DMA1 Stream2 拉低 CS、Stream3 写 SPI1 TXDR、Stream4 拉高 CS。
- `FML/DAC_FML.*`：片上 DAC 波形 DMA 输出。
- `FML/ADC_FML.*`：双ADC同步DMA缓冲、校准、帧重启、回调状态和DCache维护。
- `FML/FFT_FML.*`：4096 点 FFT、窗函数、峰值和相位基础计算。
- `FML/FREQ_FML.*`：TIM2 输入捕获自动量程、低频中断、高频 DMA、DCache 维护、超时和频率结果。
- `BLL/FFT_BLL.*`：FFT 结果结构体和主峰/次峰插值频率。
- `BLL/PHASE_BLL.*`：公共频率细化、双通道正弦拟合、三角波谐波识别与亚采样互相关时延、相位差和校准补偿。
- `BLL/ADS8688_BLL.*`：ADS8688 16 bit 原始码到伏特值的量程换算。
- `BLL/G_MODEL_BLL.*`：保存 100～3000 Hz 的 30 点模型增益，校验 100 Hz/0.1 V 档位并按固定 10 倍后级反算源端 Vpp 和 AD9910 ASF。
- `BLL/G_IIR_MODEL_BLL.*`：将分子为1的已知模型传递函数按1 MS/s双线性变换为CMSIS-DSP二阶biquad。
- `API/ADC_API.*`：ADC 数据处理、FFT 计算和串口输出入口。
- `API/FFT_API.*`：独立 FFT 输出入口，当前主循环未直接调用。
- `API/FREQ_API.*`：测频初始化、主循环处理、结果访问和 USART1 摘要输出入口。
- `API/PHASE_API.*`：当前双ADC相位差demo入口、摘要输出和逐帧重启。
- `API/AD9910_API.*`：按波形类型、频率和 mVpp 参数启动 AD9910 输出，并支持按原始 14 bit ASF 启动单 Profile 正弦；示例风格的 `AD9910_output_sine(hz, mvpp)` 中 `mvpp` 表示十倍放大后的目标幅度。GPIO、Master Reset 和基础寄存器只初始化一次，连续修改正弦参数只重写 Profile 0；从 RAM 模式切回正弦时才重新配置单 Profile 模式。
- `API/G_BASIC_API.*`：要求2不连接已知模型，按100 Hz步长直接产生最高1 MHz、末级目标3.0 Vpp的正弦信号；要求3/4使用已知模型增益表进行前馈控制。
- `API/G_CONSOLE_API.*`：USART1非阻塞行命令解析、参数校验和结果摘要；完整模式支持 `r2 <Hz>`、`r3`、`r4 <Hz> <Vpp>` 控制全部基本要求。
- `FML/G_DIGITAL_MODEL_FML.*` / `API/G_DIGITAL_MODEL_API.*`：ADC1/DAC1双缓冲DMA、IIR块处理、PB1按键消抖和PC13运行指示。
- `API/ADS8688_API.*`：CH1 demo 初始化、连续采样和 VOFA+ FireWater 输出。
- `HDL/TJC_HMI.*`：淘晶驰字符串指令发送、文本/数值赋值，以及READY、触摸、自定义命令、`0x70`文本和`0x71`数值返回帧解析。
- `API/TJC_HMI_API.*`：G题多级菜单、要求选择、共用计算器式数字键盘、参数校验、启动输出和结果页刷新入口。

## 当前引脚定义

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| USART1_TX | PB6 | USB 调试串口 TX，921600 baud |
| USART1_RX | PB7 | USB 调试串口 RX，921600 baud |
| USART3_TX | PB10 | 淘晶驰串口屏 RX，115200 baud |
| USART3_RX | PB11 | 淘晶驰串口屏 TX，115200 baud |
| G_MODEL_KEY | PB1 | 数字已知模型启动键，内部上拉、低有效 |
| G_MODEL_LED | PC13 | 数字已知模型运行指示，默认低电平点亮 |
| ADS8688_CS | PB12 | ADS8688 片选，软件控制，低有效 |
| ADS8688_RST_PD | PC4 | ADS8688 复位/低功耗控制，demo 保持高电平 |
| ADS8688_SCK | PB13 / SPI2_SCK | 硬件 SPI 时钟，17 MHz |
| ADS8688_SDO | PB14 / SPI2_MISO | ADS8688 数据输出 |
| ADS8688_SDI | PB15 / SPI2_MOSI | ADS8688 命令输入 |
| FREQ_IN / TIM2_CH1 | PA0 | 方波测频输入，上升沿，0 到 3.3 V |
| ADC1_INP1 | PA1_C | ADC1 采样输入 |
| ADC2_INP7 | PA7 | ADC2 采样输入；相位demo中使用 |
| DAC1_OUT1 | PA4 | 片上 DAC1 CH1 输出 |
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

注意：`AD9833_CS / FSYNC` 当前使用 PA1，而 ADC1 输入也配置为 PA1_C。H750 的 PA1/PA1_C 模拟开关配置需要结合实物接线确认，若两者同时使用需优先排查引脚冲突。

## 待确认

- ADS8688 demo 目前只是“已编译”，需要用 CH1 实际电压完成通信、零点、正负满量程和 VOFA+ 波形验收。
- `.ioc` 已同步 USART3（PB10/PB11）、USART1/USART3 波特率、内部 HSI 派生的 480 MHz 时钟入口、ADC2_INP7 和 TIM1 Update TRGO。PA7 在 `.ioc` 中以已实物验收的 ADC2 输入为基准；DAC8830 的 SPI1_MOSI 仍是切换任务时使用的源码级复用，CubeMX 无法同时表达二者。
- VOFA+ FireWater 依赖换行分帧，不应在同一 UART 中插入其他日志，否则会增加无关通道或造成解析干扰。

- 方波测频尚未用实物信号源覆盖验证 1 Hz、频段切换点、10 kHz 分辨率切换点和 1 MHz 上限；应重点观察串口 `raw`、`mode`、`ticks` 和 `periods`。
- 当前 TIM2 标称时钟为 75 MHz，来自内部 HSI。已按本次 1 MHz 实测做单点比例校准，但系数可能随板卡、温度和时钟条件变化；高精度场景仍建议多频点复核或使用外部高精度时钟。
- PA0 输入必须限制在 0 到 3.3 V，并与信号源共地；5 V 方波不可直接输入。

- TIM1 TRGO已改为Update，理论采样率为1 MHz；当前系统使用内部HSI派生时钟，实测前仍需用已知信号复核真实采样率。
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
- AD9910 当前用于 G 题单 Profile 正弦输出，后级按用户确认的固定10倍电压放大换算。统一幅度标定为末级满量程7646.0 mVpp对应ASF 0x3FFF，唯一人工修改入口为 `HDL/AD9910_Constants.h` 的 `AD9910_OUTPUT_FULL_SCALE_MVPP`。要求（3）和要求（4）无屏版本均已由用户确认实物验收通过；当前进入串口屏整机联调。原RAM方波/三角波代码保留但当前不调用。
- DAC8830 模块的软件换算固定为 ±10 V，原正弦 demo 参数为 10 kHz、1 Vpp、250 点；该 demo 当前不由主程序启动。
- SPI1 初始化已配置为 Master、TX-only、16-bit、MSB first、CPOL low、CPHA 1-edge、软件 NSS、/2 分频；当前 SPI123 时钟改用 PLL3，目标 120 MHz，SCK 约 60 Mbit/s。
- DAC8830 正弦模块使用 250 点表和 TIM4 DMA 定时，目标更新率 2.5 MS/s；该链路当前不运行，重新启用后仍需用示波器或逻辑分析仪确认 10 kHz 输出和阶梯/毛刺情况。

## 工作区注意

- 文档生成前工作区已有无关修改：`.clangd`、`.settings/*`、`.vscode/c_cpp_properties.json`。
- 后续开发请避免回退这些未确认来源的文件。

---
name: stm32-adc-port-test
description: 在本 STM32 信号题工程中执行“adc端口预设”：以定时器触发 ADC、DMA 单次采集默认 1024 点，完成后停止定时器、ADC 和 DMA，并在前台将整帧发送到 VOFA+。当用户说“执行adc端口预设”“adc端口预设，采样率100kHz”“采集1024点ADC数据发到VOFA”，或需要核验 ADC 通道、采样率和输入波形时使用。

---

# ADC 端口预设

将本任务实现为可启用、一次性的 ADC 端口验证，而不是连续采集业务。没有明确要求时，不编译、烧录、连接串口或启动调试器。

## 先核实工程

1. 读取 `STATUS.md`、`README.md`、`GUIDE.md` 和 `AGENTS.md`，执行 `git status --short`；保留所有来源不明的改动。
2. 用 `rg` 搜索 `HAL_ADC_Start_DMA`、`HAL_ADC_ConvCpltCallback`、`HAL_TIM_Base_Start`、`VOFA`、`FireWater`、`SCB_`、`.dma_buffer`、`MX_ADC`、`MX_TIM`、`MX_USART`，再读实际调用链、`.ioc`、`Core/Src/adc.c`、`tim.c`、`usart.c`、链接脚本和 `CMakeLists.txt`。
3. 从实际生成源和当前主任务确认 MCU、ADC 实例/通道/引脚、ADC 时钟和分辨率、采样时间、TRGO、可用 TIM/DMA/UART、DMA 数据宽度/模式、串口波特率和 D-Cache 状态。`.ioc` 可辅助核对，但若它与生成源或 `STATUS.md` 不一致，要报告差异并以当前构建源为准。
4. 检查当前任务占用的 ADC、TIM、DMA stream/request、UART 和引脚。切换到本预设时只初始化其必需外设；不要并行启动冲突的 demo、DCMI、DAC 或其他采样链路。

当前检出是 STM32H750，但此 Skill 不固定 ADC、TIM、DMA、UART 或引脚。当前工程中可优先审查已存在的 `API/ADC_VOFA_API.*`、`FML/ADC_VOFA_FML.*`、`FML/ADC_FML.*` 和 `FML/USART_FML.*`，仅在其配置与本次通道、一次性 DMA 和串口需求相符时复用。

## 解析请求与默认值

- 采样点数：用户未指定时使用 `1024`；使用 normal/one-shot DMA，不要 circular DMA。
- 采样率：采用用户明确值。未指定时，先从当前可复用测试模块或当前 ADC/TIM 配置提取合理值；无可靠依据时先询问用户，不要擅自选定。
- ADC 通道：采用用户指定的引脚、实例或通道；未指定时仅选用当前工程已配置、文档明确且不冲突的 ADC 通道，并在实施前说明选择依据。无法可靠确定时先询问。
- UART：优先当前工程用于 PC/VOFA+ 的串口和已存在发送封装，不能仅因某 UART 存在就假定其接到 PC。
- 输出协议：先搜索既有 VOFA+ 协议/封装。存在且适用时沿用其字段、单位和串口发送方式；不存在时使用单通道文本，每个原始码值一行（可用 `adc_code:<value>\r\n`），不混入普通日志。

## 采样率和转换预算

每次实现都由当前时钟实时计算，不能写死某个 PSC、ARR 或时钟频率。

1. 从 RCC/HAL 取得所选 TIM 的真实计数时钟 `f_tim`。当 APB 分频不为 1 时，核实该系列的 timer-clock 倍频规则；不要把 PCLK 直接当作计数时钟。
2. 为目标 `f_target` 搜索计数器可表示的整数 `PSC` 和 `ARR`，计算：

   ```text
   f_actual = f_tim / ((PSC + 1) * (ARR + 1))
   error_ppm = 1e6 * (f_actual - f_target) / f_target
   ```

   在本计时器位宽范围内选绝对频率误差最小的组合；优先较小 PSC 和足够大的 ARR 以保留调整余量。设置 PSC/ARR、清零 CNT、产生 UG 更新事件，并确认 `TRGO=UPDATE` 与 ADC 外部触发边沿匹配。报告目标、实际值和误差。
3. 从实际 ADC 配置取得 `f_adc`、采样时间 `Tsample_cycles`、分辨率和过采样。按该 MCU 参考手册对应分辨率的 `Tconversion_cycles` 计算：

   ```text
   f_adc_max_by_conversion = f_adc / (Tsample_cycles + Tconversion_cycles)
   ```

   对扫描序列还要乘以每次触发的转换数，并为触发同步/过采样留出余量。只有 `f_actual` 不高于该上限且满足数据手册的 ADC 时钟、采样保持和输入阻抗要求时才允许启动；否则增加采样时间、降低采样率或请用户选择。
4. 同时检查帧长度 `N / f_actual` 和 UART 传输时间。一次性采集完成后再发送，串口慢于采集不改变 ADC 采样率，但要说明发送等待时间和一次性帧间隙。

## 实现边界

1. 优先将小型测试状态机放在独立 `FML/` 模块，入口放 `API/`；用明确编译宏或单一 `main.c` 调用点启用。不要把新逻辑写入会被 CubeMX 覆盖的位置；新增 `.c` 同时加入 `CMakeLists.txt`。
2. 使用静态 DMA 缓冲区，元素宽度与 ADC DMA 配置一致。对带 D-Cache 的 MCU，把缓冲区放在 DMA 可访问的 RAM 区域（本工程优先既有 `.dma_buffer` / RAM_D2 方案），地址与长度按 cache line 对齐；不要放入 DMA 不可达的 DTCM。
3. DMA 启动前对缓冲区做 Clean+Invalidate；DMA 完成且 CPU 读取前做按范围 Invalidate。先检查 D-Cache 是否启用和所用 HAL/CMSIS API 对地址、长度的对齐约束。
4. 启动顺序为：准备状态和缓存 → 配置/停止 TIM → 启动 ADC normal DMA → 启动 TIM。DMA 完成回调只置 `capture_done` 标志（可立刻禁止 TIM 触发），不格式化、不 `printf`、不发送整组 UART 数据。
5. 在主循环/前台检测完成标志后，停止 TIM、停止 ADC DMA 并确认 DMA 不再运行；然后维护 D-Cache、转换/格式化数据并发送整帧。错误回调只记录错误码和标志，前台负责停止和报告。
6. 避免修改现有全局 HAL 回调的语义；若项目已有 ADC 回调分发，按现有活动状态路由到本模块。关闭宏、移除 `main.c` 单一入口或调用 Stop 后，应释放本预设占用的 TIM、ADC、DMA 和 UART 发送状态。

## VOFA+ 与验收

- 先复用项目现有 FireWater/VOFA+ 格式；字段使用稳定的单通道名称，例如当前通道的语义名，不要将引脚名或 ADC 分辨率猜写进协议。
- 若使用默认文本格式，VOFA+ 选择能逐行解析的文本接收；若使用 FireWater，选择 `FireWater` 并使波特率、8N1/流控与固件一致。
- 上电后只采集一帧，采满后停止采样链路，再发送该帧；不循环重启，除非用户另行要求。
- 接线验收时先断电接线、共地，并以示波器确认 ADC 引脚电压、偏置和绝对最大额定值；不要把串口接收或成功编译当作模拟输入验收。

## 验证与交付

仅当用户明确要求“编译”“构建”“烧录”“上板测试”或“自动化实物验证”时，使用仓库统一脚本；否则只给出建议命令。获准构建时运行 `Tools/build.ps1`，并执行 `git diff --check` 和 `git status --short`。获准烧录时才使用 `Tools/flash.ps1`。

完成后简要报告：

- ADC、通道和引脚；TIM、DMA、UART；
- 目标/实际采样率、误差和采样点数；
- ADC 转换时间预算和 VOFA+ 输出格式；
- 修改文件，以及启用/关闭方式；
- D-Cache、DMA 内存区域、UART 吞吐量、采样率/模拟前端限制和未做实物验证的边界。

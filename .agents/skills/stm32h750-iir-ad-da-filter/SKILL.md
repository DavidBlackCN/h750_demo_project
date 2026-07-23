---
name: stm32h750-iir-ad-da-filter
description: 在本 STM32H750 信号题工程中实现、切换、排查或验收 ADC1 到 DAC1 的实时二阶 IIR 数字低通。当用户要求复刻 `1/(1e-8 s^2 + 3e-4 s + 1)`、运行 ADC→IIR→DAC 主任务、调整实时 DMA 数据格式、排查 DAC 输出噪声或验证幅频/波形时使用。
---

# STM32H750 ADC→IIR→DAC

先读取 `STATUS.md`、`README.md`、`GUIDE.md`，执行 `git status --short`，再搜索 `IIR_FML`、`IIR_ADDA_FML`、`ADC_FML`、`adc.c`、`dac.c` 与 `dma.c`。保留不相关的工作区改动。

## 已固化链路

- 输入：ADC1，`PA1_C / ADC1_INP1`；输出：DAC1_CH1，`PA4 / DAC1_OUT1`。
- 定时器：TIM1_TRGO 触发 ADC，TIM4_TRGO 触发 DAC；默认均为 1 MS/s。
- 缓冲：1024 点循环 DMA，两个 512 点半块；缓冲置于 `.dma_buffer` 且 32 字节对齐。
- DAC1_CH1 DMA：halfword、very-high 优先级；输出缓冲必须是 `uint16_t`，并以 `(const uint32_t *)` 传入 HAL API。
- 数据格式：`ADC码 → Q15去 2048 中点 → float32 CMSIS DF1 → Q15量化 → 加回 2048 → DAC码`。不照搬参考工程的 `2048 - 25` 偏移；本任务保持单位直流增益。
- 连续滤波状态必须跨半块保持。不要逐块重新初始化 biquad。

目标传递函数：

```text
                    1
H(s) = --------------------------------
       1e-8 s^2 + 3e-4 s + 1
```

用双线性变换按实际采样率生成系数，CMSIS `arm_biquad_cascade_df1_f32` 的顺序为 `b0,b1,b2,a1,a2`。CMSIS DF1 的反馈项符号与常见分母写法相反，须保持 `IIR_Lowpass_Init()` 的实现。

## 实时调度

1. 初始化 DAC DMA、ADC DMA 后，先启动 TIM4/DAC，再启动 TIM1/ADC，使 DAC 相位领先 ADC。
2. ADC 半传输回调处理前 512 点并回写 DAC 同侧半块；全传输回调处理后 512 点并回写同侧半块。
3. 开启 D-Cache 时，CPU 读 ADC 半块前按范围 `Invalidate`；写 DAC 半块后按范围 `Clean`。长度和地址都必须是 32 字节对齐。
4. 回调中只做滤波、D-Cache 和计数；不得 `printf`、串口发送、FFT 或阻塞等待。
5. 主任务只初始化 GPIO、DMA、ADC1、DAC1、TIM1、TIM4 和 IIR API；不得同时启动 ADC2、DCMI/AD9226、DAC8830、AD9910 或冲突的 TIM1/DMA 任务。

## 切换与验证

- 正常 IIR：`IIR_ADDA_BYPASS_TEST_ENABLED = 0U`。
- 定位 ADC/DMA 链：设为 `1U`，原始 ADC 码直接写 DAC。旁路仍失真时不要先改 IIR 系数。
- 独立 DAC 对照：保持 1 MS/s、1024 点 DMA；表中必须恰好一个周期，频率为 `1e6 / 1024 = 976.5625 Hz`，避免循环边界台阶。
- ADC 输入必须处于 0～3.3 V；双极性信号经偏置、保护和低阻抗缓冲后再接 PA1。
- 若 ADC 接入导致信号源波形变形，优先处理采样保持驱动：运放缓冲，ADC 脚侧 22～100 Ω 串阻与 470 pF～2.2 nF 对地电容，并评估增加 ADC 采样时间。

验收先比较模拟参考滤波器与 PA4：正弦测 Vpp/频率/相位，三角波检查谐波衰减而非要求仍是三角形。该传递函数约 −3 dB 于 596 Hz；1 kHz 正弦理论增益约 0.505。200 Hz、1 Vpp、约 1.65 V 偏置三角波适合检验过渡带。

## 噪声定位

独立 DAC 正弦干净而 ADC→IIR→DAC 有噪声时，优先检查 ADC 前端、ADC/DAC DMA 并发和模拟供电，而非改传递函数。用稳定直流分别比较旁路/IIR：直流增益应为 1。

需要区分数字与模拟噪声时，单次抓取 1024 点 `adc_code`、量化前滤波值和 `dac_code`：

- ADC 码已抖动：输入、ADC 采样保持或参考问题；
- ADC 码平滑而 DAC 码异常：数值转换或 DMA 回写问题；
- DAC 码平滑而 PA4 有噪声：DAC、VDDA/VREF+、地回流或负载问题。

快照只在 DMA 回调中复制并置标志，串口/VOFA+ 在主循环一次性发送；不得连续打印 1 MS/s 数据。

## 交付

修改后运行：

```powershell
cmake --build --preset Debug
git diff --check
git status --short
```

仅在用户要求时烧录。报告“已编译”“已烧录”“已上板”“已验收”的边界；同步更新 `README.md`、`GUIDE.md` 和 `STATUS.md`。

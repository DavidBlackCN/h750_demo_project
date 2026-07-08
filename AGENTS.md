# Codex 开发提示

本仓库是一个基于 STM32H750 的电赛信号题模板项目。目标不是做通用框架，而是在比赛现场快速完成信号产生、采集、分析、控制和串口调试。

## 工作原则

- 先读现有分层和同类模块，再动代码。
- 不要修改无关代码，不要重构 CubeMX 生成文件。
- 新驱动优先放 `HDL/`；需要功能封装再放 `FML/`；题目业务放 `BLL/`；主循环入口放 `API/`。
- 新增 `.c` 文件后必须加入根目录 `CMakeLists.txt` 的 `target_sources()`。
- 改完必须尽量运行 `cmake --build --preset Debug`。
- 工作区可能有用户或 IDE 产生的未提交改动，不要回退。

## 常用命令

```powershell
rg --files
rg -n "keyword" API BLL FML HDL Core
cmake --build --preset Debug
git status --short
```

## 当前工程事实

- MCU：STM32H750XBHx。
- 构建系统：CMake + Ninja + arm-none-eabi-gcc。
- DSP：CMSIS-DSP，路径默认来自本机 STM32Cube H7 V1.12.1。
- 串口：USART1，PB6/PB7，921600 baud。
- 片上 DAC：DAC1 CH1 / PA4，TIM4 TRGO + DMA circular。
- ADC：ADC1 INP1 / PA1_C，DMA one-shot，目标 FFT 长度 1024。
- FFT：`FFT_LENGTH=1024`，`FFT_SAMPLE_RATE=1000000.0f`。
- DMA 缓冲区使用 `.dma_buffer`，链接到 RAM_D2，注意 DCache 清理。

## 现有外部芯片

- AD9833：`HDL/AD9833.*` + `HDL/MySPI.*`，当前 main 已启动 1 kHz 正弦。
- AD9910：`HDL/AD9910.*`，已有基础寄存器/Profile 操作，主程序未启用。
- DAC8830：`HDL/DAC8830.*` + `HDL/DAC8830_SPI.*`，默认软件 SPI 引脚为 `CS1=PE2`、`CS2=PE0`、`SDI=PA7`、`SCLK=PA5`。

## 已知风险

- ADC 外部触发和 TIM1 TRGO 配置可能不匹配，详见 `STATUS.md`。
- ADC DMA 当前不是连续处理链路，处理一帧后会停止。
- 串口输出原始数组和频谱数组数据量大，实时任务中要谨慎使用。
- STM32H750 片内 FLASH 只有 128 KB，新增库或大量字符串前先关注 map 和构建输出。

## 文档维护

- 新增模块后更新 `README.md` 的能力列表。
- 改使用方法或接线后更新 `GUIDE.md`。
- 更改代码后及时维护 `STATUS.md`，尤其是当前状态、引脚定义、已知问题和实测结果。
- 实测结果、已知问题、当前风险写入 `STATUS.md`。

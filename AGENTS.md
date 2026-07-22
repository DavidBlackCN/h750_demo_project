# Codex 开发提示

本仓库是面向电赛信号题的 STM32H750 模板工程，优先服务比赛现场的快速开发、切换主任务和实物调试。

## 先读信息源

不要在本文档重复维护会变化的工程事实。开始任务前按需读取：

- `STATUS.md`：当前主程序行为、已实现模块、时钟/引脚/外部芯片等工程事实、资源占用、已知风险、实测结果和工作区注意事项。以它作为当前状态的首要信息源。
- `README.md`：项目能力概览、目录结构、默认启动流程和构建入口。
- `GUIDE.md`：具体功能的使用方法、接线、测试流程和依赖说明。
- `.agents/skills/`：已固化的专项实现流程。任务命中某个 Skill 时先读完对应 `SKILL.md`。

## 工作流程

1. 先用 `rg --files` 和 `rg -n "keyword" API BLL FML HDL Core` 找现有分层、同类模块和实际调用链，再动代码。
2. 只修改用户任务相关代码，不顺手重构或格式化无关文件。
3. 先执行 `git status --short`识别用户、IDE 或旧任务的未提交改动；不回退、覆盖或删除来源不明的内容。
4. 切换主任务时，仅在 `main.c` 启动当前任务必需的外设和 API；保留其他 demo 代码，并根据 `STATUS.md` 检查引脚、DMA、定时器和内存冲突。
5. 优先完成最小可测闭环，再增加校准、过滤、超时和诊断；串口默认只输出验收必需的摘要。

## 分层与生成代码

- 新芯片驱动和底层时序放 `HDL/`。
- 稳定的硬件功能封装放 `FML/`。
- 数据换算、算法和题目业务放 `BLL/`。
- 主循环调用的应用入口放 `API/`。
- 新增 `.c` 文件后加入根目录 `CMakeLists.txt` 的 `target_sources()`。
- 不主动重新生成 CubeMX 代码，不大范围重写生成文件。必须手工改动时保持最小差异，并在 `STATUS.md` 记录与 `.ioc` 可能不一致的部分。

## 验证与交付

按改动风险执行：

```powershell
cmake --build --preset Debug
git diff --check
git status --short
```

- 代码改动尽量完成 Debug 构建；若构建环境缺失，明确说明未验证项。
- 检查链接输出中的 FLASH/RAM 占用，不要只看编译是否通过。
- 硬件、精度或时序相关结论必须区分“已编译”、“已上板”、“已校准”和“已验收”，不用仿真或单点数据代替实物验收。
- 交付时说明实际修改文件、验证结果和仍需用户复测的内容。

## 自动编译与烧录

本工程的 Debug 预设使用 Ninja 和 `cmake/gcc-arm-none-eabi.cmake`，VS Code 配置指定 `cube-cmake`。新建构建目录、工程改名或 CMake 缓存失效时，先在根目录执行：

```powershell
$cubeBin = "C:/Users/DavidBlackCN/.vscode/extensions/stmicroelectronics.stm32cube-ide-core-1.4.0-win32-x64/resources/binaries/win32/x86_64"
$cubeCmake = "C:/Users/DavidBlackCN/.vscode/extensions/stmicroelectronics.stm32cube-ide-build-cmake-1.46.0-win32-x64/resources/cube-cmake/win32/x86_64/cube-cmake.exe"
$env:PATH = "$cubeBin;$env:PATH"
& $cubeCmake --preset Debug
```

若 STM32Cube VS Code 扩展升级导致上述版本目录变化，先从 `.vscode/settings.json` 的 `cmake.cmakePath` 确认仍使用 `cube-cmake`，再分别在 `stmicroelectronics.stm32cube-ide-core-*` 下定位 `cube.exe` 所在目录、在 `stmicroelectronics.stm32cube-ide-build-cmake-*` 下定位 `cube-cmake.exe`，不回退使用已删除的旧路径。

平时增量编译：

```powershell
cmake --build --preset Debug
```

烧录使用 OpenOCD + DapLink/CMSIS-DAP，参数与 `.vscode/launch.json` 保持一致。确认 Debug 构建成功、开发板与 DapLink 连接正确后执行：

```powershell
& "D:/CodeStudy/OpenOCD-20260302-0.12.0/bin/openocd.exe" `
  -s "D:/CodeStudy/OpenOCD-20260302-0.12.0/share/openocd/scripts" `
  -f "interface/cmsis-dap.cfg" `
  -f "target/stm32h7x.cfg" `
  -c "program build/Debug/adc_fft_demo.elf verify reset exit"
```

需要自动完成“编译成功后再烧录”时，使用 PowerShell 的退出码检查，不在编译失败时继续下载旧 ELF：

```powershell
cmake --build --preset Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "D:/CodeStudy/OpenOCD-20260302-0.12.0/bin/openocd.exe" `
  -s "D:/CodeStudy/OpenOCD-20260302-0.12.0/share/openocd/scripts" `
  -f "interface/cmsis-dap.cfg" `
  -f "target/stm32h7x.cfg" `
  -c "program build/Debug/adc_fft_demo.elf verify reset exit"
```

- 烧录前检查 ELF 路径、本次构建时间和 OpenOCD 识别的目标，避免下载旧固件或选错调试器。
- 仅在用户已要求烧录、上板测试或自动化实物验证时执行 OpenOCD；单纯代码审查或编译任务不默认改写板上 FLASH。
- OpenOCD 输出必须同时出现 programming/verify 成功与 reset 完成，才能报告“已烧录”；只生成 ELF 不等于已下载。

## 文档维护

- 新增或删除能力时更新 `README.md`。
- 使用方法、接线或测试流程变化时更新 `GUIDE.md`。
- 当前主任务、工程事实、引脚/时钟/外部芯片、资源占用、已知风险、实测结果或工作区注意变化时更新 `STATUS.md`。
- 专项实现流程经过实物验收并具有复用价值时，再更新 `.agents/skills/` 中的对应 Skill，并运行 Skill 验证器。

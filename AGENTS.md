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
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1
git diff --check
git status --short
```

- 一般情况下，AI 不自行执行编译或烧录；仅在用户明确要求“编译”“构建”“烧录”“上板测试”或“自动化实物验证”时，才调用下述统一脚本。否则只报告尚未执行的验证项。
- 用户明确要求编译时调用 `Tools/build.ps1`；若构建目录新建、工程改名或 CMake 缓存失效，向脚本传递 `-Reconfigure`。代码改动后需要验证但用户未授权执行时，应说明建议运行的命令。
- 检查链接输出中的 FLASH/RAM 占用，不要只看编译是否通过。
- 硬件、精度或时序相关结论必须区分“已编译”、“已上板”、“已校准”和“已验收”，不用仿真或单点数据代替实物验收。
- 交付时说明实际修改文件、验证结果和仍需用户复测的内容。

## 自动编译与烧录

本节仅在用户明确要求编译、烧录、上板测试或自动化实物验证时适用；不得因为代码改动而自行执行。所有命令从仓库根目录调用，不能在文档或对话中拼接本机绝对路径。

本机路径配置只允许放在被 Git 忽略的 `Tools/config/local.ps1`，不得写入本文件。

用户要求编译时执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1
```

新建构建目录、工程改名或 CMake 缓存失效时：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\build.ps1 -Reconfigure
```

用户明确要求烧录时，确认开发板和 DapLink/CMSIS-DAP 连接正确后执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Tools\flash.ps1
```

脚本会自行处理本机配置、构建前置条件和烧录结果校验；只生成 ELF 不等于已下载。

## 文档维护

- 新增或删除能力时更新 `README.md`。
- 使用方法、接线或测试流程变化时更新 `GUIDE.md`。
- 当前主任务、工程事实、引脚/时钟/外部芯片、资源占用、已知风险、实测结果或工作区注意变化时更新 `STATUS.md`。
- 专项实现流程经过实物验收并具有复用价值时，再更新 `.agents/skills/` 中的对应 Skill，并运行 Skill 验证器。

# G题淘晶驰串口屏开发指南

本文对应当前 STM32H750 固件中的 `HDL/TJC_HMI.*` 和
`API/TJC_HMI_API.*`。串口屏型号按现有硬件
`TJC1060X570_011C_I` 编写，界面分辨率为 1024×600。

动态数据由 STM32 使用 ASCII 下发，页面上的固定标题可以直接在 USART HMI
中写中文。建议先完成本文的纯色简易界面并联调成功，再替换背景图片、颜色和字体。

官方参考资料：

- [淘晶驰指令集](http://wiki.tjc1688.com/commands/index.html)
- [page 页面跳转](http://wiki.tjc1688.com/commands/page.html)
- [printh 发送固定十六进制数据](http://wiki.tjc1688.com/commands/printh.html)
- [get/串口屏返回数据格式](http://wiki.tjc1688.com/return/index.html)
- [文本控件](http://wiki.tjc1688.com/widgets/Text.html)
- [数字控件和 covx 转换](http://wiki.tjc1688.com/widgets/Number.html)

## 1. 接线与串口设置

| 串口屏端 | STM32H750端 | 说明 |
| --- | --- | --- |
| RX | PB10 / USART3_TX | TX、RX交叉连接 |
| TX | PB11 / USART3_RX | TX、RX交叉连接 |
| GND | GND | 必须共地 |
| 电源 | 独立合适电源 | 按屏幕铭牌供电，不建议由开发板小电流接口供电 |

必须使用 TTL 串口方式，不是 RS232 电平。通讯参数：

```text
115200 baud
8 data bits
1 stop bit
no parity
no flow control
```

USART1 继续作为电脑调试口，USART3只连接串口屏。

完整题目模式下，USB串口和串口屏同时有效。USART1命令为：

```text
r2 1000
r3
r4 1000 1.5
```

两路入口共享同一套输出控制；若先后从两路启动不同参数，后收到的有效操作覆盖前一次输出。

## 2. USART HMI工程全局设置

1. 新建工程，设备选择 `TJC1060X570_011C_I`，分辨率1024×600。
2. 制作并导入至少一套中文字体和一套较大的数字字体。
3. 工程字符编码建议保持GB2312。STM32动态下发内容目前全部为ASCII，不会产生中文编码冲突。
4. 不启用主动解析模式。
5. 在 `program.s` 中配置以下内容，`page home` 必须放在最后：

```text
bauds=115200
bkcmd=0
recmod=0
page home
```

6. 本项目按钮使用自定义 `printh` 帧，按钮的“发送键值”选项不要勾选，否则会额外产生 `0x65` 触摸帧。
7. 所有按钮指令均写在“弹起事件”，不要同时写在按下事件和弹起事件。

## 3. 页面层级

必须创建以下5个页面，页面名称要完全一致：

```text
home
└─ menu
   ├─ control
   │  └─ keypad
   └─ result
```

- `home`：欢迎页。
- `menu`：基本要求2/3/4选择页。
- `control`：参数确认和启动页。
- `keypad`：频率和Vpp共用的计算器式数字输入页。
- `result`：运行结果和核心换算数据显示页。

控件坐标、颜色和背景图可以自由设计，但下面列出的页面名、控件名和事件指令不能随意修改。

## 4. home欢迎页

创建控件：

| 控件类型 | 控件名 | 建议初始文字 |
| --- | --- | --- |
| 文本 | `t_title` | 电路模型探究装置 |
| 文本 | `t_subtitle` | STM32H750 + AD9910 |
| 按钮 | `b_enter` | 进入系统 |

`b_enter` 弹起事件：

```text
printh 5A A5 01 FF FF FF
```

STM32收到命令后跳转到 `menu` 页面。

## 5. menu要求选择页

创建控件：

| 控件类型 | 控件名 | 建议初始文字 |
| --- | --- | --- |
| 文本 | `t_title` | 功能选择 |
| 按钮 | `b_r2` | 基本要求2 |
| 按钮 | `b_r3` | 基本要求3 |
| 按钮 | `b_r4` | 基本要求4 |
| 按钮 | `b_home` | 返回首页 |

各按钮弹起事件：

```text
// b_r2
printh 5A A5 02 FF FF FF

// b_r3
printh 5A A5 03 FF FF FF

// b_r4
printh 5A A5 04 FF FF FF

// b_home
printh 5A A5 33 FF FF FF
```

选择后由STM32跳转到 `control` 页面并刷新参数。

## 6. control参数页

创建控件：

| 控件类型 | 控件名 | 用途 |
| --- | --- | --- |
| 文本 | `t_mode` | 显示 REQUIREMENT 2/3/4 |
| 文本 | `t_freq` | 显示当前频率，例如 1000 Hz |
| 文本 | `t_vpp` | 显示目标幅度，例如 2.0 Vpp |
| 文本 | `t_hint` | READY、VALUE UPDATED等提示 |
| 按钮 | `b_freq` | 打开频率数字键盘 |
| 按钮 | `b_vpp` | 打开Vpp数字键盘 |
| 按钮 | `b_start` | 启动输出 |
| 按钮 | `b_back` | 返回功能选择 |

按钮弹起事件：

```text
// b_freq
printh 5A A5 10 FF FF FF

// b_vpp
printh 5A A5 11 FF FF FF

// b_start
printh 5A A5 30 FF FF FF

// b_back
printh 5A A5 31 FF FF FF
```

STM32会根据所选要求自动隐藏不允许修改的按钮：

| 要求 | 频率 | Vpp |
| --- | --- | --- |
| 要求2 | 可修改：100 Hz～1 MHz，步长100 Hz | 固定3.0 Vpp |
| 要求3 | 固定1 kHz | 固定2.0 Vpp |
| 要求4 | 可修改：100～3000 Hz，步长100 Hz | 可修改：1.0～2.0 Vpp，步长0.1 V |

## 7. keypad计算器式输入页

### 7.1 需要的控件

| 控件类型 | 控件名 | 用途 |
| --- | --- | --- |
| 文本 | `t_key_title` | INPUT FREQUENCY或INPUT TARGET VPP |
| 文本 | `t_value` | 当前输入字符串，`txt_maxl`建议设为12 |
| 文本 | `t_unit` | Hz或Vpp |
| 按钮 | `b_0`～`b_9` | 数字键 |
| 按钮 | `b_dot` | 小数点，仅Vpp输入时显示 |
| 按钮 | `b_clear` | 全部清除 |
| 按钮 | `b_ok` | 确认 |
| 按钮 | `b_cancel` | 取消 |

页面布局建议采用四列计算器布局：

```text
┌──────────────────────────────┐
│ INPUT FREQUENCY       Hz     │
│          1000                │
├──────┬──────┬──────┬────────┤
│  7   │  8   │  9   │ CLEAR  │
│  4   │  5   │  6   │ CANCEL │
│  1   │  2   │  3   │   OK   │
│  0   │  .   │      │        │
└──────┴──────┴──────┴────────┘
```

### 7.2 数字按钮事件

每个数字按钮的弹起事件向 `t_value.txt` 追加对应字符：

```text
// b_0
t_value.txt+="0"

// b_1
t_value.txt+="1"

// b_2
t_value.txt+="2"
```

`b_3`～`b_9`按同样方法配置。

`b_dot` 弹起事件：

```text
t_value.txt+="."
```

Vpp只允许一个小数点和一位小数。若重复输入小数点或格式超出范围，STM32会拒绝并把 `t_key_title` 显示为 `INVALID INPUT`，此时按CLEAR重新输入。

`b_clear` 弹起事件：

```text
t_value.txt=""
```

`b_ok` 弹起事件：

```text
printh 5A A5 20 FF FF FF
```

STM32收到OK后发送：

```text
get keypad.t_value.txt
```

串口屏自动返回 `0x70 + ASCII文本 + FF FF FF`，STM32解析、检查范围和步长，然后返回 `control` 页面。

`b_cancel` 弹起事件：

```text
printh 5A A5 21 FF FF FF
```

### 7.3 输入示例

| 功能 | 正确输入 | 错误输入示例 |
| --- | --- | --- |
| 要求2频率 | `100`、`1000`、`1000000` | `150`、`1000001` |
| 要求4频率 | `100`、`1700`、`3000` | `50`、`1750`、`3100` |
| 要求4 Vpp | `1.0`、`1.5`、`2.0` | `0.9`、`1.55`、`2.1` |

## 8. result结果页

创建控件：

| 控件类型 | 控件名 | 显示内容 |
| --- | --- | --- |
| 文本 | `t_status` | RUNNING或ERROR |
| 文本 | `t_mode` | 当前要求 |
| 文本 | `t_freq` | 当前频率 |
| 文本 | `t_target` | 模型目标Vpp |
| 文本 | `t_gain` | 要求3/4显示当前模型增益；要求2固定显示N/A |
| 文本 | `t_source` | 探究装置十倍放大后输出mVpp |
| 文本 | `t_asf` | AD9910 ASF码 |
| 按钮 | `b_again` | 返回参数页继续调整 |
| 按钮 | `b_home` | 返回首页 |

建议在各动态文本旁增加静态中文标签，例如“频率”“目标幅度”“模型增益”“源端幅度”“ASF码”。

按钮弹起事件：

```text
// b_again
printh 5A A5 32 FF FF FF

// b_home
printh 5A A5 33 FF FF FF
```

## 9. 自定义命令表

所有屏幕按钮命令格式为：

```text
5A A5 命令字 FF FF FF
```

| 命令字 | 功能 |
| --- | --- |
| `01` | 欢迎页进入菜单 |
| `02` | 选择要求2 |
| `03` | 选择要求3 |
| `04` | 选择要求4 |
| `10` | 编辑频率 |
| `11` | 编辑Vpp |
| `20` | 数字键盘确认 |
| `21` | 数字键盘取消 |
| `30` | 启动输出 |
| `31` | 参数页返回菜单 |
| `32` | 结果页返回参数页 |
| `33` | 返回首页 |

## 10. 下载和联调步骤

1. 在USART HMI中执行“编译”，先消除全部错误和警告。
2. 使用串口下载或SD卡方式把生成的TFT文件下载到屏幕。
3. 屏幕断电，按第1节连接USART3，确认共地后再上电。
4. 烧录当前STM32 Debug固件。
5. 上电后屏幕应停留在 `home` 页面；STM32保持空闲，不启动任何要求，也不输出默认波形。
6. 依次测试 `home → menu → 要求4 → 频率键盘 → Vpp键盘 → START → result`。
7. 电脑同时打开USART1调试串口。串口屏每次启动输出后，USART1应看到类似：

```text
HMI R4 f=1000Hz target=2.0Vpp source=791.9mVpp ASF=1696 status=OK
```

ASF会随 `AD9910_OUTPUT_FULL_SCALE_MVPP` 的实测标定值变化。

## 11. 常见问题

### 屏幕有画面但按钮无响应

- 检查屏幕TX是否接PB11、屏幕RX是否接PB10。
- 检查是否共地、是否为TTL电平。
- 检查按钮代码是否写在弹起事件。
- 检查 `printh` 中每个十六进制数是否都是两位，并包含末尾三个 `FF`。
- 检查是否误勾选“发送键值”。

### MCU能切页但文本不更新

- 页面名和控件名必须与本文完全一致，名称区分大小写。
- 检查 `bkcmd=0`、`recmod=0`。
- 检查文本控件 `txt_maxl` 是否足够。
- 先不要使用中文动态文本，确认ASCII数据显示正常后再处理编码。

### 输入后显示INVALID INPUT

- 频率必须是100 Hz整数步长。
- 要求4频率只能在100～3000 Hz。
- 要求4 Vpp只能在1.0～2.0 Vpp，并且只保留一位小数。
- 按CLEAR后重新输入，不要重复输入小数点。

### 屏幕反复重启或触摸时黑屏

大尺寸串口屏瞬时电流较大，优先检查供电电压、供电电流能力和线材压降，不要只根据电源指示灯判断供电正常。

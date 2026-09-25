# CANable USB识别与版本查询

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-24，用户将CANable插入电脑后，Windows读取结果：

- USB串行设备COM11，Status=OK，ProblemCode=0，系统usbser驱动。
- VID/PID：16D0/117E。
- USB设备描述：`CANable2 b158aa7 github.com/normaldotcom/canable2.git`。
- 以115200打开COM11，仅发送版本查询 `V\r`，实际回复：`16e7497-dirty github.com/normaldotcom/canable2.git\r`。
- 查询后关闭串口。没有发送打开CAN通道或CAN报文的命令，没有刷写固件。

USB描述中的b158aa7与版本命令回复的16e7497-dirty不同，按原值分别保留，不能据此断言它与某一上游提交完全一致。

依据设备CDC枚举、版本命令响应及[上游SLCAN固件说明](https://github.com/normaldotcom/canable2-fw)，下一阶段采用现有SLCAN路径，在Ubuntu中经slcand接入SocketCAN；当前不需要为了枚举而改刷candleLight。这只是接入方案，尚未验证Ubuntu USB透传、slcand接口或物理CAN收发。

原始证据：[设备属性](device-properties.json)、[版本查询](version-query.json)。本次没有拍摄新的实物照片或设备管理器截图。

## 后续用户实物照片

用户随后提供的原始照片已保存：[CANable外壳与端子](user-canable-case.jpg)、[开发板J40区域](user-board-j40.jpg)。CANable外壳标识MKS CANable V2.0，端子为GND/CANH/CANL，顶部有对应120Ω的OFF/ON标记和外露拨动件；按该实物操作，不再默认要求拆壳加120R跳线帽。照片没有证明开关触点或终端电阻已经导通。

开发板J40一列看起来已有蓝色跳线帽，保留此视觉判断的有限置信度，不记为已测通。当前仍没有总线接线完成、电阻测量或物理收发证据。

后续纠正：用户现场确认J40是裸露金属针，因此上段“看起来已有帽”的视觉判断不成立。助手指导断电插帽，用户随后回复“已经插上了”；记录为用户操作反馈，尚无插后照片或电气检查。详见[操作答疑](../../docs/项目操作答疑.md)。

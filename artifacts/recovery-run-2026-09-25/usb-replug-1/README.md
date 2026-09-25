# 实际USB拔插与受控恢复：通过

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-25，用户实际拔下并插回CANable USB，并在VMware中将设备重新连接到Ubuntu。助手负责断开监测、接口与Qt重连、板端诊断、复位和新查询验证。原始自动程序不读取聊天，其physical_unplug_user_confirmation=false原样保留；真实用户确认单独记录在[acceptance.json](acceptance.json)，不修改原始[result.json](result.json)。

## 实际过程

1. 断开前正常查询成功2ms，can0的ifindex=4。
2. 用户拔下USB，旧can0消失、心跳超时；随后查询正确返回transport_error，未显示成功。结果中connected=true仅表示旧Qt句柄状态，online=false且写入报错才是此时的实际可用性，不能只看connected字段判断设备在线。
3. 用户插回，Windows识别同一CANable为COM11；当时尚未交回虚拟机。用户在VMware连接到Ubuntu后，root一次性助手识别相同序列号并创建新的can0（ifindex=5、UP/LOWER_UP），Qt重新建立连接和日志会话。
4. 板端仍无心跳。DAP读取同一芯片与程序前缀，诊断fault=4、last_hal_error=32。项目所用旧HAL定义0x20为HAL_CAN_ERROR_ACK；固件遇错误进入停止状态，需复位恢复。
5. 助手经DAP执行一次SYSRESETREQ，无烧录、无额外3秒停机。随后有效心跳恢复，新会话seq1查询2ms成功。

旧会话与新会话序号分别计数；恢复后的seq1属于新SocketCAN连接和新日志，不能与断开前seq1混为同一个请求。两段原始日志已分别核对。

- [断开前与断开期间原始记录](before-session/frames.jsonl)
- [重连后原始记录](after-session/frames.jsonl)
- [故障状态和DAP恢复动作](../usb-replug-1-reset/fault-action.json)
- [实际Qt窗口截图](passed.png)
- [独立验收结论](acceptance.json)

## 边界与当前状态

这是一轮实际USB拔插后的受控恢复，包含人手拔插、VMware归属选择、事先sudo启动的接口助手，以及助手执行的板端复位。已证明离线检测、错误反馈、接口重建、Qt重连和恢复后新请求均可完成；尚未实现固件在所有CAN故障下自主恢复，也不声称全程无人干预。

一次性root恢复助手已经退出。普通Qt操作窗口已重新打开并连接新的can0，见[当前部署](../deployment-after-usb.json)。本轮用户配合动作已完成，不需要重复测试。

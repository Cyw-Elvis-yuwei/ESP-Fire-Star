# Qt 真实 CAN 界面验收（已完成）

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-25，用户在 Ubuntu Qt 界面完成真实 `can0` 操作。实际链路：Qt → SocketCAN/SLCAN → CANable → CAN 总线 → STM32F103；本轮没有启动模拟节点。

## 验收结果

| 项目 | 结果与证据 |
| --- | --- |
| 关灯、开灯、查询 | 用户确认步骤1～5正常；逐帧匹配0x321请求与0x322回复，前9条全部成功，2～3ms |
| 板端系统复位 | 助手经DAP执行一次SYSRESETREQ，不写Flash；不是用户按实体RESET，也不是断电测试 |
| 复位检测 | GUI于UTC 10:06:04.003记录一次restart_observed |
| 复位后查询 | 用户seq=10/op=2查询成功，4ms，应答LED=0（默认关） |
| 保存报告 | 用户回复“已保存”；报告生成于UTC 10:07:07.974，10条全部成功，0失败、0超时 |
| 日志状态 | 0传输错误、0日志错误；首个复位后低计数样本暂按stale处理，后续递增样本确认重启，stale_heartbeats=1 |

逐条核对：[acceptance.json](acceptance.json)。复位动作：[reset-command.json](reset-command.json)、[reset.log](reset.log)。前9条核对：[normal-control-check.json](normal-control-check.json)。

## 原始报告的分类错误与修复

原始 [report.json](original-session/report.json)、[report.md](original-session/report.md)、[frames.jsonl](original-session/frames.jsonl) 原样保留，SHA256在acceptance.json。原报告simulated=true是界面分类缺陷：Qt5.12的SocketCAN isVirtual仅判断sysfs路径是否含virtual，真实SLCAN的can0同样位于 /sys/devices/virtual/net/can0。

依据：[Qt5.12.8官方源码](https://raw.githubusercontent.com/qt/qtserialbus/v5.12.8/src/plugins/canbus/socketcan/socketcanbackend.cpp)。修复读取ip -j -d link的链路类型，区分vcan/vxcan与物理CAN/SLCAN，不按名称猜测。原始报告未改写，本文件和acceptance.json单独记录审计结论。

修复版Ubuntu Qt5.12.8编译通过，分类测试15项通过（含初始化/清理、12组分类和1组实际can0测试）。只接收真实心跳的GUI检查收到15帧、0条发送，新报告simulated=false，实际窗口截图已检查。见[修复验证](../identity-fix-2026-09-25/README.md)。这份新的0命令报告只验证分类修复，不冒充用户10条命令验收。

hardware_verified=false保持为软件报告的保守字段：软件无法自行确认实物灯和接线。真机验收结论由请求/应答、DAP复位记录、实物观察反馈共同支持，不能单纯改字段充当验收。

## 当时交付状态与阶段边界

已换成修复后的Ubuntu界面（部署时PID5481），连接真实can0，只接收心跳，不自动发命令。原10条会话已归档；新窗口从0开始属于新会话，不用重做。

本轮完成真实控制、查询、一次系统复位后的新查询与报告保存。后续持续查询和实际USB受控恢复分别记录于[持续报告](../continuous-run-2026-09-25/README.md)和[恢复报告](../recovery-run-2026-09-25/README.md)；自动固件故障恢复、Bootloader与升级仍未实现。此前vcan结果保留为独立模拟证据。

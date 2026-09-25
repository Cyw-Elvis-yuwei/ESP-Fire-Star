# 真机故障检测与恢复阶段

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

## 最新结果：两种真机恢复场景均已验证

- DAP受控暂停节点约3秒、复位后新查询：通过。
- 用户实际USB拔插、VMware交回Ubuntu、重建can0、Qt重连、DAP复位及新查询：通过，见[完整USB恢复记录](usb-replug-1/README.md)。

现场读到板端fault=4 / HAL_CAN_ERROR_ACK，复位后查询2ms成功。属于受控恢复，不冒称固件自恢复或完全无人干预。一次性root助手已退出，普通窗口已恢复。本文件汇总最终结果，准备时的待办已归入修订前备份。

2026-09-25。按用户授权由助手完成工具操作，用户只负责本地sudo输入、物理USB拔插与VMware设备分配；这些配合已完成。

## 已完成：真实节点无响应与重启恢复

新增RecoveryRun复用DiagnosticEngine。Qt“节点故障恢复检查”执行正常查询 → 等待心跳离线 → 验证一次查询超时 → 等待有效心跳 → 使用新序号查询 → 保存报告。心跳恢复本身不算恢复通过；旧查询回复不能满足新查询。每个等待阶段有60秒上限，异常或取消不算通过。

该按钮要求CANable及can0持续存在。它不主动制造故障，也不自动刷固件或复位；故障及恢复动作由外部受控操作提供，证据独立附上。适配器消失会明确报adapter_disconnected_not_covered，不将其误记为该用例通过。

助手在实际开发板上经DAP暂停CPU约3秒，再SYSRESETREQ复位并恢复执行，未写Flash。核对相同芯片身份、已验证程序向量前缀，操作成功后状态SLEEPING属于WFI正常等待。

| 检查 | 本轮结果 |
|---|---|
| 正常查询 seq1 | 成功，13ms |
| 心跳停止 | 检测到一次heartbeat_timeout |
| 故障期间查询 seq2 | 预期timeout，主机事件处理时记录517ms（配置500ms） |
| 恢复 | 一次restart_observed，随后有效心跳 |
| 新查询 seq3 | 成功，2ms |
| 总体 | 节点恢复用例通过；2次成功和1次预期失败构成完整用例 |
| 日志/传输错误 | 均0 |

[原始结果](physical-session/recovery-20260925-105146-407-6dc0b12a/result.json)、[可读报告](physical-session/recovery-20260925-105146-407-6dc0b12a/result.md)、[原始报文](physical-session/frames.jsonl)、[DAP动作](fault-action.json)、[独立审计](audit.json)、[真实界面截图](physical-recovery-pass.png)均保留。未把调试暂停写成物理拔线；physical_unplug_verified仍为false。

构建通过；恢复逻辑12项、持续逻辑13项、正常逻辑14项、诊断核心28项通过（均含初始化/清理）。恢复测试覆盖没有故障/没有恢复的截止时间、心跳恢复但查询失败、太快恢复而未产生预期超时、迟到旧回复、取消、适配器离线及日志/报告失败。故障软件分支使用受控时钟模拟，实际节点用例另有上表真机证据。

## 已完成：真实USB拔插与受控恢复

此用例由独立Qt验收程序协调，不属于普通GUI的节点恢复按钮。恢复助手只识别固定串号CANABLE_SERIAL_REDACTED、VID16d0/PID117e，并核对旧slcand；等待移除/返回各最多300秒，不接管未知接口。用户先在Ubuntu以sudo启动助手，再按观察程序提示完成拔插。

| 阶段 | 实际证据 |
|---|---|
| 拔出前 | 新查询2ms成功，can0 ifindex=4 |
| USB移除 | 旧can0消失、心跳超时；查询transport_error，未记成功 |
| 重新插入 | Windows先识别COM11；用户通过VMware交回Ubuntu |
| 接口恢复 | 同一适配器被重新识别，新can0 ifindex=5且UP；Qt重新连接 |
| 板端诊断 | 无心跳，fault=4、last_hal_error=32（0x20 ACK） |
| 板端恢复 | DAP执行SYSRESETREQ，无Flash写入，无额外3秒故障暂停 |
| 恢复验收 | 有效心跳及新会话seq1查询2ms成功 |

[USB完整过程](usb-replug-1/README.md)、[原始结果](usb-replug-1/result.json)、[用户配合与综合验收](usb-replug-1/acceptance.json)、[板端复位动作](usb-replug-1-reset/fault-action.json)分别留档。旧会话与新会话序号独立计数，恢复后seq1不能与断开前seq1混为一条请求。

原始程序不读取聊天，physical_unplug_user_confirmation=false保留；用户确认另存acceptance.json，不改自动结果。节点用例的physical_unplug_verified=false也保持，因为那一轮确实没有拔USB。

## 交付与限制

一次性root助手已退出，普通GUI已恢复，部署快照见[deployment-after-usb.json](deployment-after-usb.json)。此处是验收结束时状态，不保证未来重启后仍有同一PID、设备号或接口。

已完成故障检测、错误反馈、接口重建、Qt重连及恢复后新查询。恢复包含人工拔插/VMware分配、脚本和DAP复位，不是固件自恢复或完全无人干预。不需要为了补用户点击记录重复已通过的自动真机测试。日常重启操作见[使用与恢复指南](../../docs/使用与恢复指南.md)。

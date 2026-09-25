# 第一轮真实CAN基线验收

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-25：**STM32 CAN候选已下载并校验，真实can0收到心跳，查询/开灯/再查询成功，用户确认三色LED绿色常亮。** 本轮使用新购独立平台，与此前vcan模拟证据分开保存。

## 证据链

| 层次 | 实际结果 |
|---|---|
| 用户接线 | 用户报告GND/CANH/CANL已接好，J40跳线帽已插 |
| 用户量测 | 用户用UT136B+回报CANH-CANL为59.4～59.5Ω；文字反馈，无仪表照片 |
| CANable | 同序列号CANABLE_SERIAL_REDACTED，正常16d0:117e，Ubuntu ttyACM0；slcand请求500kbps，can0为实际SLCAN接口，非vcan |
| 应用备份 | 512KB两次完整读取一致；哈希与前一阶段LED全Flash校验值一致 |
| 下载与校验 | 7044字节CAN候选，前8192字节四扇区擦写；整片512KB读回一致，镜像外字节和选项字节保留 |
| 固件运行 | 短暂停核快照读取boot_stage=3，fault=0、HAL error=0；恢复执行约150ms后HAL tick增加157ms |
| 板到电脑 | 独立被动采集记录995条合法0x123心跳；用于连通性证据，未作为压力测试 |
| 双向命令 | seq0x2501查询LED关；0x2502开灯；0x2503再次查询LED开，3条完整应答均匹配 |
| 用户实物确认 | 用户回答“绿色常亮”，已单独保存；未编造助手拍摄的实物照片 |

## 命令实测

| 命令 | 序号 | 应答内容（hex） | 主机观测耗时 |
|---|---|---|---|
| 查询状态 | 0x2501 | 01 02 01 25 00 00 00 00 | 1.351 ms |
| 开灯 | 0x2502 | 01 01 02 25 00 01 00 00 | 1.064 ms |
| 查询状态 | 0x2503 | 01 02 03 25 00 01 00 00 | 1.547 ms |

耗时是这次Ubuntu用户态往返测量，不作为确定性实时延迟承诺。控制通过Python标准库SocketCAN短时探测发送，已有只允许vcan的CLI回归保护保持原样，没有绕过它对真实总线运行故障注入。

## 原始记录

- [真实命令与接口报告](report.json)、[请求/应答原始帧](frames.jsonl)。报告生成时还未收到用户视觉回复，因此physical_led_user_confirmed=false保留原快照。
- [用户绿灯确认](user-led-observation.json)、[本轮综合验收与哈希](acceptance.json)。后续用户确认以这两份为准。
- [被动心跳记录](heartbeat-capture.jsonl)。
- [下载及调试问题记录](../can-flash-2026-09-25/README.md)、[最终运行核验](../can-flash-2026-09-25/runtime-verification.json)。
- [CAN候选源码](../../firmware/stm32-can/README.md)。BIN SHA256：`231407bd9f23191dc61f92862b31c3241791c1bdfcd7c4599b46c537a6503b1e`。

Ubuntu原记录目录：`/home/demo/can-diagnostic-bench/diagnostics/artifacts/physical-exchange-20260925-uly2cj0v`，被动采集目录 `physical-can-20260925-57ot3g_7`。Windows目录为本轮备份，不代表整个工程与Ubuntu同步。

## 阶段范围与后续闭环

本轮只证明首次真实CAN心跳和三条命令往返，不是Qt界面操作验收。此后[Qt用户验收](../physical-gui-2026-09-25/README.md)、[7步正常回归](../normal-run-2026-09-25/README.md)、[60秒持续查询](../continuous-run-2026-09-25/README.md)及[受控恢复](../recovery-run-2026-09-25/README.md)均独立留档。

当时启动GUI等待用户操作的步骤已经结束，不再作为待办。固件CAN错误锁存停发、需要复位这一限制仍存在；没有Bootloader、升级或长期可靠性验收。当前完整使用方法见[使用指南](../../docs/使用与恢复指南.md)。

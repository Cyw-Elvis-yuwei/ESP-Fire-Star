# CAN 正常回归报告

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../../../publication/README.md)。

接口：can0；模拟：false

协议结果：**passed**

只验证本轮协议控制与查询。实物灯需人工观察；不包含负载、断线恢复、复位或升级。失败/停止后不继续发恢复命令，LED状态请重新查询。

| 步骤 | 结果 | 序号 | 耗时ms |
|---|---|---:|---:|
| 读取初始状态 | success | 1 | 1 |
| 关闭LED | success | 2 | 2 |
| 查询并确认关闭 | success | 3 | 1 |
| 打开LED | success | 4 | 2 |
| 查询并确认开启 | success | 5 | 2 |
| 恢复初始状态 | success | 6 | 1 |
| 查询并确认已恢复 | success | 7 | 13 |

原始报文：/home/demo/can-diagnostic-bench/diagnostics/artifacts/gui-20260925-102412-077/frames.jsonl

起止UTC：2026-09-25T10:24:12.136Z ～ 2026-09-25T10:24:14.547Z

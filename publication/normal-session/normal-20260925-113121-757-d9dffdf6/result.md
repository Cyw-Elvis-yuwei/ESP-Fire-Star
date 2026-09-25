# CAN 正常回归报告

接口：can0；模拟：false

协议结果：**passed**

只验证本轮协议控制与查询。实物灯需人工观察；不包含负载、断线恢复、复位或升级。失败/停止后不继续发恢复命令，LED状态请重新查询。

| 步骤 | 结果 | 序号 | 耗时ms |
|---|---|---:|---:|
| 读取初始状态 | success | 1 | 3 |
| 关闭LED | success | 2 | 2 |
| 查询并确认关闭 | success | 3 | 1 |
| 打开LED | success | 4 | 2 |
| 查询并确认开启 | success | 5 | 12 |
| 恢复初始状态 | success | 6 | 2 |
| 查询并确认已恢复 | success | 7 | 9 |

原始报文：/home/demo/can-publication-20260925/clean-build/artifacts/gui-20260925-113121-646/frames.jsonl

起止UTC：2026-09-25T11:31:21.757Z ～ 2026-09-25T11:31:24.152Z

# 节点故障恢复检查

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../../../publication/README.md)。

结果：**passed**；接口：can0；模拟：false

保持CAN适配器及接口存在。检查节点心跳消失、一次查询超时，再以恢复后的新查询确认通信。故障原因与恢复动作须另附证据；不等于USB拔插自动重连。

| 阶段 | 序号 | 结果 | 响应ms |
|---|---:|---|---:|
| baseline_query | 1 | success | 13 |
| probing_outage | 2 | timeout | 517 |
| verifying_recovery | 3 | success | 2 |

起止UTC：2026-09-25T10:51:46.407Z ～ 2026-09-25T10:52:12.742Z

原始报文：/home/demo/can-diagnostic-bench/diagnostics/artifacts/gui-20260925-105146-397/frames.jsonl

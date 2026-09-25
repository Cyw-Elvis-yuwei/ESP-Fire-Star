# CAN diagnostic session

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../../../publication/README.md)。

- Simulated: true
- Hardware verified: **false**
- Interface: vcan0
- Connected: true
- Online (fresh heartbeat): true
- Log healthy: true
- Log error: 

Software protocol observations only. Virtual CAN does not verify physical CAN, STM32 firmware, wiring or the physical LED. Restart events are observations from counters and uptime; a new matching command is needed to demonstrate recovery.

## Counters

| Counter | Total |
| --- | ---: |
| commands_attempted | 2 |
| commands_failed | 1 |
| commands_rejected | 0 |
| commands_sent | 2 |
| commands_succeeded | 1 |
| commands_timeouts | 1 |
| duplicate_heartbeats | 0 |
| heartbeat_timeouts | 0 |
| ignored_frames | 0 |
| invalid_frames | 0 |
| local_echo_frames | 0 |
| log_errors | 0 |
| restart_observations | 0 |
| results_dropped | 0 |
| rx_frames | 30 |
| stale_heartbeats | 0 |
| transport_errors | 0 |
| tx_frames | 2 |
| unmatched_responses | 1 |
| valid_heartbeats | 28 |

## Recent command results

At most 256 results retained; counters cover the whole session.

| Sequence | Operation | Success | Outcome | Latency ms |
| ---: | ---: | --- | --- | ---: |
| 1 | 1 | false | timeout | 505 |
| 2 | 2 | true | success | 1 |

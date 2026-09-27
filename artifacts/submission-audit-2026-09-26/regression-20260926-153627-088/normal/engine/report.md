# CAN diagnostic session

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
| commands_attempted | 4 |
| commands_failed | 0 |
| commands_rejected | 0 |
| commands_sent | 4 |
| commands_succeeded | 4 |
| commands_timeouts | 0 |
| duplicate_heartbeats | 0 |
| heartbeat_timeouts | 0 |
| ignored_frames | 0 |
| invalid_frames | 0 |
| local_echo_frames | 0 |
| log_errors | 0 |
| restart_observations | 0 |
| results_dropped | 0 |
| rx_frames | 31 |
| stale_heartbeats | 0 |
| transport_errors | 0 |
| tx_frames | 4 |
| unmatched_responses | 0 |
| valid_heartbeats | 27 |

## Recent command results

At most 256 results retained; counters cover the whole session.

| Sequence | Operation | Success | Outcome | Latency ms |
| ---: | ---: | --- | --- | ---: |
| 1 | 1 | true | success | 0 |
| 2 | 2 | true | success | 0 |
| 3 | 1 | true | success | 1 |
| 4 | 2 | true | success | 0 |

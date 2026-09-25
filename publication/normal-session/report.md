# CAN diagnostic session

- Simulated: false
- Hardware verified: **false**
- Interface: can0
- Connected: false
- Online (fresh heartbeat): false
- Log healthy: true
- Log error: 

Software protocol observations only. Virtual CAN does not verify physical CAN, STM32 firmware, wiring or the physical LED. Restart events are observations from counters and uptime; a new matching command is needed to demonstrate recovery.

## Counters

| Counter | Total |
| --- | ---: |
| commands_attempted | 7 |
| commands_failed | 0 |
| commands_rejected | 0 |
| commands_sent | 7 |
| commands_succeeded | 7 |
| commands_timeouts | 0 |
| duplicate_heartbeats | 0 |
| heartbeat_timeouts | 0 |
| ignored_frames | 0 |
| invalid_frames | 0 |
| local_echo_frames | 0 |
| log_errors | 0 |
| restart_observations | 0 |
| results_dropped | 0 |
| rx_frames | 32 |
| stale_heartbeats | 0 |
| transport_errors | 0 |
| tx_frames | 7 |
| unmatched_responses | 0 |
| valid_heartbeats | 25 |

## Recent command results

At most 256 results retained; counters cover the whole session.

| Sequence | Operation | Success | Outcome | Latency ms |
| ---: | ---: | --- | --- | ---: |
| 1 | 2 | true | success | 3 |
| 2 | 1 | true | success | 2 |
| 3 | 2 | true | success | 1 |
| 4 | 1 | true | success | 2 |
| 5 | 2 | true | success | 12 |
| 6 | 1 | true | success | 2 |
| 7 | 2 | true | success | 9 |

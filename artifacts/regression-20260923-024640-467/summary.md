# Simulated CAN acceptance

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

Result: **PASS**

Interface: `vcan0` (verified vcan only).

Simulation evidence only; physical CAN and STM32 hardware are not verified.

| Case | Result | Detail |
|---|---|---|
| normal | PASS | Expected behavior and recovery observed |
| response-timeout | PASS | Expected behavior and recovery observed |
| heartbeat-timeout | PASS | Expected behavior and recovery observed |
| restart | PASS | Expected behavior and recovery observed |
| wrong-seq | PASS | Expected behavior and recovery observed |
| late-response | PASS | Expected behavior and recovery observed |

Fault-injection cases pass only when the expected fault is detected and a new matching GET_STATUS succeeds.

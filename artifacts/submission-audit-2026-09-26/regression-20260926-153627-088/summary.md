# Simulated CAN acceptance

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

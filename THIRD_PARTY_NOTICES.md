# Third-party sources and notices

This repository combines project-specific code with third-party components. No blanket license grant is applied to all files by this publication.

- Qt SerialBus CAN example: Qt5.12.8; original BSD notice retained in the example-derived files under `app/`. Qt libraries have their own licensing terms and are installed separately.
- STM32 HAL, CMSIS and startup code: pinned Embedfire example commit `6afb695f68d33c2a4e84b1cc1147fd8a422912f6`. Original file notices are preserved under `firmware/stm32-can/vendor`; see [NOTICE.txt](firmware/stm32-can/NOTICE.txt) and [UPSTREAM.json](firmware/stm32-can/UPSTREAM.json).
- Early LED example: original source notices and [source manifest](firmware/led-baseline/UPSTREAM.json) retained.
- Linux SocketCAN, can-utils, Qt libraries, pyOCD, ARM Compiler and device packs are external tools/dependencies; no compiler or device-pack binaries are distributed here.

See [source and contribution boundaries](docs/UPSTREAM.md). Public source visibility does not replace checking each applicable license for your intended use.

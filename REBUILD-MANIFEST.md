# ESP-Fire Star 重建学习清单

## 身份

- 项目：CAN设备诊断与自动回归测试台。
- 归档方式：独立项目仓库，无合集编号。
- 仓库：https://github.com/Cyw-Elvis-yuwei/ESP-Fire-Star
- 分支：main；精确源码版本以所读文件的Git提交为准。
- 本地归档：E:/TEMPLATE/ESP-Fire-Star；建议练习目录E:/TEMPLATE/esp-fire-star-learning（未创建）。
- [GitHub清单](https://github.com/Cyw-Elvis-yuwei/ESP-Fire-Star/blob/main/REBUILD-MANIFEST.md)

## 硬件与构建边界

STM32F103ZET6霸道V2，PB8 CAN RX、PB9 CAN TX，PB0绿色LED低有效，J40收发器供电。单个CANable和500kbit/s经典CAN。未使用板载摄像头、显示屏、存储或网络模块。

本工程不是CubeMX生成工程，无.ioc。实际ARM入口[build.ps1](firmware/stm32-can/build.ps1)，主程序[main.c](firmware/stm32-can/main.c)，厂家原Keil工程保存在vendor中作来源参考；未把原uvprojx直接构建作为已验证路径。

## 函数导航与练习顺序

| 顺序 | 文件 | Ctrl+F锚点 | 操作位置与观察 |
|---|---|---|---|
| 1 | [app_protocol.c](firmware/app_protocol.c) | app_protocol_handle | 函数定义；请求校验、幂等设置与回复生成；先用native_test理解字节 |
| 2 | [app_heartbeat.c](firmware/app_heartbeat.c) | app_heartbeat_poll | 函数定义；100ms条件与回绕；验证不补发积压心跳 |
| 3 | [can_app.c](firmware/stm32-can/can_app.c) | canbench_init | 函数定义；引脚、500k位时序、过滤器和IRQ |
| 4 | [can_app.c](firmware/stm32-can/can_app.c) | canbench_poll | 函数定义；有限队列、单帧在途、回复优先和fault停发 |
| 5 | [diagnosticengine.cpp](core/diagnosticengine.cpp) | DiagnosticEngine::receiveResponse | 函数定义；序号/操作/状态匹配，再看checkTimeouts |
| 6 | [normalrun.cpp](core/normalrun.cpp) | NormalRun::start | 函数定义；7步编排与原状态恢复 |
| 7 | [continuousrun.cpp](core/continuousrun.cpp) | ContinuousRun::start | 函数定义；完整样本与延迟统计 |
| 8 | [recoveryrun.cpp](core/recoveryrun.cpp) | RecoveryRun::start | 函数定义；离线、预期超时、恢复后新查询 |

同名HAL函数在vendor与tests/stubs中用途不同：ARM链接真实HAL，主机适配测试链接替身，不能编辑错文件。led-baseline是历史独立固件，不是当前CAN主程序。

## 证据与学习边界

源文件、构建、烧录、设备验收分别见[证据索引](docs/验收与证据索引.md)。历史答案库成功不算新练习成功；每轮重建须先做软件验证，再按硬件条件完成针对性观察。由AI辅助完成的代码不表示学习者已经独立掌握。

本次公开副本只对目标选择改用环境变量，另将主机测试编译器默认值改为PATH中的clang.exe，其余业务/GUI/固件源文件保持原字节。设备备份不公开，不拿脱敏档案执行下载。

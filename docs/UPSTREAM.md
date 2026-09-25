# 复用来源、改编范围与贡献边界

本项目优先复用已有Qt示例、Linux CAN工具链和厂家HAL。本文对照本地来源清单及源码整理；2026-09-25这次报告修订没有重新下载或检索上游。

## 1. Qt与Linux

Qt界面基线来自Ubuntu软件包`qtserialbus5-examples` 5.12.8-0ubuntu1，原目录为`/usr/lib/x86_64-linux-gnu/qt5/examples/serialbus/can`。用户原练习`/home/demo/can-diagnostic-bench/can`保留；项目运行副本为同级`diagnostics`。

复用连接对话框、SerialBus收发、资源和报文表；新增诊断面板、DiagnosticEngine接入及测试编排。`app/`保留示例原有BSD版权条款；Qt库本身的许可与示例源码许可不同。

Linux SocketCAN、vcan、SLCAN、can-utils直接使用已有实现，没有自研内核驱动。现有CANable固件已支持SLCAN，本项目未重新刷写它；固件版本字符串含dirty，不能据此给它编造唯一的干净上游提交。

历史参考：[Qt CAN示例说明（5.15文档，实际代码5.12.8）](https://doc.qt.io/archives/qt-5.15/qtserialbus-can-example.html)、[Ubuntu源码包目录](https://archive.ubuntu.com/ubuntu/pool/universe/q/qtserialbus-everywhere-src/)、[Linux SocketCAN文档](https://www.kernel.org/doc/html/latest/networking/can.html)。

## 2. STM32厂家基线

上游为[野火霸道HAL例程固定提交](https://github.com/Embedfire-stm32f103-badao/ebf_stm32f103_badao_hal_code/tree/6afb695f68d33c2a4e84b1cc1147fd8a422912f6)，提交`6afb695f68d33c2a4e84b1cc1147fd8a422912f6`。

| 使用部分 | 来源与改编 |
|---|---|
| 早期独立LED | #7寄存器点灯，4个原文件；新增链接布局和本机构建脚本；仅用于此前平台验证 |
| 当前CAN | #39双机通讯，51个原文件；复用HAL CAN V1.1.1、CMSIS、启动/system文件 |
| 项目适配 | main.c与can_app.c适配时钟、引脚、协议帧、有限队列、中断/前台调度及错误停发 |
| 可移植业务 | app_protocol/app_heartbeat共享实现，直接参与ARM构建及原生测试 |

精确文件来源与哈希见[CAN清单](../firmware/stm32-can/UPSTREAM.json)和[LED清单](../firmware/led-baseline/UPSTREAM.json)。原始vendor字节及版权声明保留，汇总通知见[NOTICE.txt](../firmware/stm32-can/NOTICE.txt)。没有重新实现STM32 CAN寄存器驱动。

## 3. 项目新增内容

GUI和CLI共用DiagnosticEngine；NormalRun、ContinuousRun、RecoveryRun编排不同检查，不各自重写请求匹配逻辑。Python模拟节点使用标准库AF_CAN，实现本项目自定义命令、心跳和模拟故障。恢复助手组合udev、slcand、ip及已有pyOCD调试能力，并限制目标身份和动作范围。

新增的价值是把现成收发能力组织为明确的成功/失败判定、重复测试、统计和留档。具体实现与边界见[初版报告](初版项目报告.md)。这不能写成“独立开发完整Qt框架、CAN驱动或下载器底层”。

## 4. 参与方式与后续发布

助手参与实现、审查、构建、工具操作和文档；用户完成硬件连接、必要的本地权限输入、部分GUI操作及物理动作/灯状态确认。证据按实际执行者记录，项目通过不自动证明用户能独立重写或解释全部代码。

本轮仅整理本地项目报告，没有执行开源发布、许可证审计或商业授权确认。后续分发应保留对应第三方通知，分别检查Qt库、示例源码、厂家文件和本机编译工具的适用条款；来源可追溯不等于自动获得任意商业用途许可。

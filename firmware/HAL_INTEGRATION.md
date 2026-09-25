# STM32业务与HAL集成说明

2026-09-25更新。本文件描述已经构建、下载并完成物理CAN验证的实现；早期设计示意全文保存在[文档修订前备份（仅本地保留）](../publication/OMITTED.md)。源码入口为[main.c](stm32-can/main.c)、[can_app.c](stm32-can/can_app.c)。

## 1. 业务与硬件分工

| 文件 | 职责 |
|---|---|
| [app_protocol.c](app_protocol.c) | 校验标准请求帧、解析命令、更新逻辑LED、生成回复；不访问GPIO |
| [app_heartbeat.c](app_heartbeat.c) | 按100ms间隔生成最新心跳；不调用CAN发送 |
| [main.c](stm32-can/main.c) | SystemCoreClock刷新、HAL/LED/时钟初始化、CAN初始化、poll与WFI |
| [can_app.c](stm32-can/can_app.c) | 旧HAL句柄、引脚/位时序/过滤器、中断、队列、调度、物理LED及故障停发 |
| [build.ps1](stm32-can/build.ps1) | 明确源码列表、链接范围与ARM产物 |

协议字节唯一说明在[PROTOCOL.md](../docs/PROTOCOL.md)，这里不维护第二套格式。业务状态由调用者持有，无业务动态分配；主循环处理协议，中断主要搬运帧和记录完成/错误事件。

## 2. 旧HAL与初始化

使用固定厂家CAN例程的HAL CAN V1.1.1：`CanTxMsgTypeDef`、`CanRxMsgTypeDef`、`HAL_CAN_Transmit_IT`、`HAL_CAN_Receive_IT`与完成回调。不能直接混用新HAL的`HAL_CAN_Start`或`HAL_CAN_AddTxMessage`。

启动路径：SystemInit → main → SystemCoreClockUpdate → HAL_Init → RGB初始化为关 → HSE/PLL配置 → canbench_init → canbench_poll/WFI。厂家SystemInit返回时用HSI，但SystemCoreClock初值为72MHz，因此先刷新该变量，再由HAL初始化SysTick。

配置HSE8MHz、PLL×9、HCLK72MHz、APB1 36MHz。CAN1重映射PB8 RX/PB9 TX；36MHz ÷ 8 ÷ (1+5+3) = 500kbit/s，SJW=1TQ。硬件过滤目标标准数据请求，软件进一步核对类型/ID/DLC。

已验证500kbit/s实际互通，未测晶振精度或物理位波形。板侧J40供电、终端与布线见[使用指南](../docs/使用与恢复指南.md)。

## 3. 接收、发送与中断

接收中断复制帧到固定RX队列后重新使能接收，不向前台暴露HAL接收缓冲区指针。RX16槽留一槽区分空/满，有效容量15；满时丢弃并计数。前台每轮最多处理4帧，回复队列8槽，先保证可容纳回复再消费请求。

一次仅一个发送在途，设置在途状态后启动旧HAL异步发送，避免完成中断早于调用返回造成状态错乱。回复优先于心跳；HAL_BUSY保留待发项，下轮重试；完成事件后才移除回复队头。心跳只保留最新候选，不补发积压周期。

向量为`USB_LP_CAN1_RX0_IRQHandler`、`USB_HP_CAN1_TX_IRQHandler`、`CAN1_SCE_IRQHandler`，共用HAL句柄，抢占优先级均为5。当前固件不同时使用MCU本身USB设备功能；板载CH340是独立USB串口器件。

主循环调用canbench_poll后WFI；SysTick及CAN中断唤醒处理。无主循环阻塞发送，也不在IRQ内等待、打印或执行完整恢复流程。

## 4. 错误与受控恢复

初始化、接收使能、提交发送、CAN错误或250ms发送等待超限触发故障。前台锁存fault，停止CAN相关中断、请求取消邮箱并点红灯，等待复位；不无限重试掩盖失败。硬件ABOM不等于应用故障状态会自动清除。

真实USB拔插后曾读到fault=4、HAL错误0x20（ACK）；恢复can0后由DAP进行SYSRESETREQ，重新出现心跳且新查询成功。该路径属于受控恢复，不是固件自动重连。按RESET或DAP复位会清空本次运行的逻辑LED/计数；本版没有持久化boot ID或参数。

PB0绿色、PB5红色、PB1蓝色，均低电平亮。逻辑LED=1映射PB0拉低；错误红灯与业务绿灯是不同含义。

## 5. 验证层次与阅读路径

- [原生业务测试](native_test.c)：不依赖真实HAL，历史139项检查通过。
- [适配测试入口](stm32-can/test-host.ps1)：11组HAL替身场景，不替代真实寄存器与IRQ验证。
- [ARM构建记录](../artifacts/can-arm-build-2026-09-24/README.md)：真实vendor HAL参与构建。
- [下载与运行核验](../artifacts/can-flash-2026-09-25/README.md)：完整备份、窄写入、读回和独立运行检查。
- [真机验收索引](../docs/验收与证据索引.md)：协议往返、用户灯确认、正常/持续/恢复。

源码、构建、真机行为与用户掌握程度分别记录。当前文档整理未重新执行上述测试。许可证通知与具体上游文件见[复用说明](../docs/UPSTREAM.md)。

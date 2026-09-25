# LED真机基线：备份、下载、恢复与用户验收

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-24，用户明确要求开始“先备份原程序，再下载已编译的LED例程”。最终结果：**下载及完整读回校验通过，程序运行检查通过，用户确认三色LED绿色常亮。**

本阶段验证独立LED基线；CAN固件及真实CAN通信尚未验证。软件首版使用用户原厂配套SWD排线、开发板独立USB供电；没有改回杜邦散线。

## 结果与证据

| 验证项 | 结果 | 证据 |
|---|---|---|
| 目标确认 | 相同探针、相同MCU唯一标识、0x414高密度STM32F1、512KB Flash | [备份元数据](backup.json) |
| 原程序备份 | 完整512KB，连续两次读取完全一致，保存后SHA256一致 | [原始Flash（仅本地保留）](../../publication/OMITTED.md)、[备份日志](../led-backup-2026-09-24.log) |
| 下载范围 | 第一个2KB擦除页；其中692字节为LED程序，其余1356字节由可信备份恢复 | [最终下载日志](program.log)、[实际首个页内容（仅本地保留）](../../publication/OMITTED.md) |
| 全Flash读回 | 512KB全部与“原备份合并LED程序”的期望内容一致 | [工具结果](program-result.json)、[实际读回（仅本地保留）](../../publication/OMITTED.md) |
| 保留内容 | LED镜像范围外的Flash字节、选项字节全部保持原值 | [工具结果](program-result.json) |
| 运行检查 | 系统复位后CPU运行，PC=0x08000298位于main，PB0配置输出低电平 | [工具结果](program-result.json) |
| 物理现象 | 用户回答“绿色常亮” | [用户观察记录](physical-led-observation.json) |

机器结果生成时尚未收到用户回答，因此 `program-result.json` 中 `physical_led_confirmed=false` 是当时的真实快照；后续实物确认单独保存在 `physical-led-observation.json`，不改写原始机器结果。

SHA256：

- 原始512KB备份：`00450a0fa2c364f125943dda3edb93b223e34775248b2f48050db3a90ebe1f77`
- 692字节LED BIN：`d659ad7df99108cf90f1b5005c441315b53cfa3c67c3b10e37ebaa704f22716a`
- 下载后的完整512KB期望值与实际读回：`a86a77e1fcb81e61dfd756d7672377e6dd3cd4dde78528d5a58d1bbd1e49653a`

备份涵盖MCU内部主Flash。不是开发板全部存储器的整机镜像；没有自动执行原程序恢复。

## 实际执行方式

复用pyOCD0.45.1和本机 `Keil.STM32F1xx_DFP/2.4.1` 中精确目标STM32F103ZE及 `STM32F10x_512.FLM`。算法擦除粒度2KB、编程页粒度1KB。没有重新构建已审核的LED产物，也没有整片擦除、修改选项字节或解除读保护。

两份项目工具分别用于备份和下载：

- `tools/backup_stm32_flash.py`：确认芯片身份，暂时停核运行以获得一致快照，完整读取两次，校验保存文件，然后恢复原先运行状态。
- `tools/program_led_baseline.py`：校验备份、芯片标识和LED产物SHA；使用新的调试会话并先显式系统复位/停核，按已确认的首个页范围写入，完整读回验证后系统复位运行，最后观察CPU与PB0寄存器。脚本需要显式 `--execute`，保护已有备份和结果。

本阶段结束时板上为LED程序，后续已替换为CAN固件。上述脚本保留了本次操作的身份与镜像限制，不作为任意板卡通用烧录器；正常接续无需再次运行。

## 本轮两处问题与实际恢复

### 1. 精确器件包连接失败，尚未擦写

初次在精确器件包目标上关闭 `pack.debug_sequences.enable` 后出现No ACK。对照实验中，generic目标仍可读到芯片；保持100kHz/attach，只切换为精确器件包则失败，启用器件包初始化序列后恢复。

本地源码核对发现，pyOCD0.45.1判断预定义序列存在后，虽然全局禁用使序列没有执行，调用方仍认为它已经执行，从而跳过内置的SWJ握手/调试上电。已检查本机Pack序列：DebugDeviceUnlock只是只读CheckID，DebugCoreStart设置调试寄存器；采用正常Pack支持路径，保留auto_unlock=False。未改pyOCD库源码。

证据：[初次失败结果](program-result-attempt1.json)、[初次失败日志](program-attempt1.log)、[generic对照](connection-isolation.log)、[精确Pack关闭序列](connection-exact-pack.log)、[恢复序列后的读取](connection-pack-sequence-enabled.log)。注意Commander可能在初始化报错时仍退出0，判断读取成功必须看到预期寄存器输出，不能只看退出码。

### 2. 首页擦除后出现HardFault，编程未完成

第二次已连通并完成写前比对，擦除首个2KB页后，算法运行出现HardFault。现场读到PC=0xFFFFFFFE、IPSR=3、VTOR=0x08000000、PRIMASK=0、SysTick仍启用，并确认第一页已全FF。

源码核对确认：直接调用FileProgrammer API不会执行CLI层的load.pre_reset选项，默认算法准备只halt，不清原应用的SysTick/NVIC状态。现场与“原厂中断状态干扰算法”一致；没有保存逐指令跟踪，因此不把具体中断触发路径说成已完全证明。

恢复采用新会话和Flash对象，显式SYSRESETREQ复位并暂停芯片。选项字节在异常现场一度读成FF，复位后恢复为与备份一致的值，没有对选项字节执行恢复或写入操作。检查首个页以外Flash仍与备份一致后，用可信备份合并LED的**完整2KB首个页**重新写入，恢复原有尾部数据。最终全Flash及选项字节验证通过。

证据：[第二次失败结果](program-result-attempt2.json)、[第二次日志](program-attempt2.log)、[异常现场](failed-algorithm-snapshot.json)、[复位前后状态](reset-recovery-snapshot.json)、[最终成功结果](program-result.json)。

这次是下载工具接入中的失败与恢复，不属于项目实现了CAN在线升级或Bootloader升级中断恢复。保持原有小型专项范围。

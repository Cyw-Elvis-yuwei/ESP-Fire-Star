# CAN下载、完整校验与调试通道恢复记录

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

**最终状态（2026-09-25）：已完成CAN候选下载，整片Flash及选项字节校验通过；独立运行核验通过，真实CAN查询/开灯/再查询成功，用户确认绿色常亮。** 下文先保留失败过程，后列恢复及验收，不能把早期暂停状态当作当前状态。

2026-09-25，用户已启用实际can0；助手SSH复核UP/LOWER_UP，slcand PID3165、ttyACM0、500kbps参数，收发计数当时均0。

已检查既有CAN候选7044字节BIN及配套HEX/AXF元数据，新增 `tools/program_can_baseline.py` 复用已验证LED下载顺序，保留原LED工具不变。新工具限定该候选SHA256、四个2KB扇区、同设备身份及完整备份/选项字节比对；准备阶段先完成语法检查；此后实际执行了preflight和下载，结果见下文。实际写入时的脚本快照与后续修正版分开保存，不能把后续修正版说成再次完成了Flash写入验证。

## 备份失败过程

- 执行 `.venv-pyocd/Scripts/python.exe tools/backup_stm32_flash.py --output artifacts/can-flash-2026-09-25`。
- 采用generic cortex_m、attach、1MHz，备份开始前暂停CPU。进度仅到192/512KB，日志自17:43:30后数分钟未更新。没有完成首轮512KB，未生成original-flash.bin、option-bytes-before.bin或backup.json。
- Windows仍枚举DAP为正常。助手核对进程身份后停止本轮卡住的备份实际Python进程PID5652；上层任务退出-1。[原始进度](backup-stalled-attempt1.log)保留。
- 新进程用100kHz generic attach尝试status/read32身份/continue/status，外层20秒超时；输出仅有设置SWD时钟，没有任何身份或运行状态结果。[诊断日志](after-stall-probe.log)保存。不能声称continue已成功。
- 首次失败处理结束时，相关Python进程已退出，尚未执行擦写或选项字节修改。那个时间点不能确认CPU恢复状态，Flash仍为此前LED程序；后续恢复与下载见下一节。

## 当时提出的恢复步骤（已结束）

当时建议重插DAP USB后复位，再读身份与少量Flash验证调试通道。用户表示找不到RESET，后续工具恢复了连接并继续执行，未把用户按下实体按钮作为成功事实。该建议是故障过程记录，不是现在需要重新操作的待办。完整备份通过后才进入实际CAN写入。

错误根因尚未确认，现有证据支持“调试命令通道无响应”，不直接宣称DAP损坏、芯片损坏或板载Flash错误。原厂备份与已验收LED产物仍保存在2026-09-24目录。

## 恢复后备份与下载

1. 用户表示找不到RESET。再次100kHz attach得到HALTED及正确芯片ID，continue成功后读到Running，见[replug-check.log](replug-check.log)。不再要求用户寻找按钮。
2. 本机pyOCD提供cmsis_dap.limit_packets兼容选项。备份保持1MHz与原16KB读取块，只限制单个在途命令，外层120秒超时，89.08秒内完成两遍512KB，内容完全一致。见[backup-attempt2-command.json](backup-attempt2-command.json)、[backup-attempt2.log](backup-attempt2.log)。这支持该配置可用，但没有唯一证明最初卡住的底层原因。
3. [backup.json](backup.json)及[完整备份（仅本地保留）](../../publication/OMITTED.md)哈希为 `a86a77e1fcb81e61dfd756d7672377e6dd3cd4dde78528d5a58d1bbd1e49653a`，与上一阶段下载LED后的全Flash一致。原来的工厂程序备份仍在2026-09-24目录。
4. [preflight.json](preflight.json)确认7044字节候选、8192字节授权扇区范围，最后一页剩余1148字节由本次可信备份保留。整个镜像之外的字节在全Flash读回中仍一致。
5. 实际擦除8192字节/4扇区，写入8192字节/8编程页；读回SHA256为 `5aadd16e242b08b37966f712a9046d5d597e406bb6de616e1732670c90db5b65`。原始[program-result.json](program-result.json)里三项读回/保留校验均true。[program.log](program.log)混有本机GBK异常路径文字，原字节保留，不能把外层UTF-8解码失败当成擦写失败。

## 运行判定的纠正，不重新烧录

本次执行版本[program-script-used.py（仅本地保留）](../../publication/OMITTED.md)只把RUNNING认作运行，实际固件主循环使用WFI，常处于SLEEPING。因此原始program-result保留success=false、runtime_check_failed和命令退出1；它同时已读到boot_stage=3、fault=0、tx_completed=4。不是Flash校验失败，也没有因此重刷。

后续读UID时发现运行/睡眠状态下该地址返回全FF，暂停CPU后恢复正确UID；部分SRAM标量在睡眠读取时也得到0，初次运行核验误失败。分别保留runtime-uid-diagnosis.log、runtime-uid-halted.log、runtime-verification-sleep-uid-attempt.log和runtime-asleep-ram-attempt.json/log，不把这些值解释为UID损坏、程序被清空或真实时基停止。

最终验证改为两次短暂停核快照，中间恢复执行150ms：候选Flash内容一致、UID正确，状态前后SLEEPING，HAL tick增加157ms，boot_stage=3，rx_frames=3、tx_completed=2089、HAL error=0、fault=0。见[runtime-verification.json](runtime-verification.json)及[runtime-verification.log](runtime-verification.log)，退出0。此工具没有复位或擦写；完成后继续运行。program_can_baseline.py已修正未来运行状态判定并限制在途包，但更新版没有再次执行写入。

## 最终证据边界

[第一轮真实CAN验收](../physical-can-2026-09-25/README.md)包含Ubuntu实际can0心跳、三次命令往返和用户绿色常亮确认。旧program-result.json的false是历史检查器结果，后续成功由独立运行核验和物理通信证据支持，不改写原始记录。所有本轮问题和解释同时记入项目操作答疑。

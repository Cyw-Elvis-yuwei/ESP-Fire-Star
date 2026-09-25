# STM32F103ZE CAN固件

截至2026-09-25，本固件已完成ARM构建、备份后下载、全Flash与选项字节校验，以及真实CAN手动控制、正常回归、持续查询和受控故障恢复验证。当前项目使用本CAN固件；独立LED程序仅是此前的平台基线。证据见[验收索引](../../docs/验收与证据索引.md)。本次文档修订没有再次构建或刷写。

## 1. 来源与构建边界

复用厂家提交 `6afb695f68d33c2a4e84b1cc1147fd8a422912f6` 的 `39-CAN通讯/CAN—双机通讯`。51个选取原文件保持原字节，来源、Git blob及SHA256见[UPSTREAM.json](UPSTREAM.json)，通知见[NOTICE.txt](NOTICE.txt)。采用该例程旧HAL CAN V1.1.1、CMSIS和启动文件，不混入其他例程的HAL版本。

[build.ps1](build.ps1)显式列出实际参与构建的文件；厂家User文件与uvprojx作为来源参考保留。实际编译器为本机ARM Compiler5.05，不等于已验证厂家原uvprojx指定的AC5.06/DFP2.3组合。可移植业务层直接使用上一级的[app_protocol.c](../app_protocol.c)与[app_heartbeat.c](../app_heartbeat.c)。

## 2. 固定配置

| 项目 | 当前实现 |
|---|---|
| 芯片 | STM32F103ZET6；Flash 0x08000000起512KB，内部RAM64KB |
| 时钟 | HSE8MHz、PLL×9、HCLK72MHz、APB1 36MHz；HAL_Init前刷新SystemCoreClock |
| CAN | PB8 RX / PB9 TX重映射；Prescaler8、BS1=5、BS2=3、SJW=1；500kbit/s |
| 接收 | 标准数据帧0x321，软件再检查flags、ID和DLC8 |
| 应答/心跳 | 0x322应答；100ms目标周期0x123心跳 |
| 队列 | RX16槽、有效容量15；回复8槽；每次前台最多处理4帧 |
| 发送 | 单个在途帧、回复优先；完成后移除回复队头；HAL_BUSY保留重试 |
| 心跳积压 | 只保留最新待发候选，不补发旧心跳；计数表示生成次数 |
| LED | 初始化RGB熄灭；绿色PB0低电平亮，逻辑1为开 |
| 错误 | 锁存fault、停CAN中断、请求取消邮箱并亮红灯；修复条件后复位恢复 |

500kbit/s已经与实际CANable互通；未用仪器测量晶振精度和位时序。开启硬件ABOM不等于应用具备自动恢复能力。250ms发送等待上限、错误回调、接收重启或发送提交失败都可触发停发。USB断开验收实际出现ACK错误，恢复总线后仍需复位，见[恢复报告](../../artifacts/recovery-run-2026-09-25/README.md)。

无RTOS、Bootloader、升级协议或CAN FD。业务层无动态内存分配、主循环无阻塞发送，WFI等待中断。启动文件预留1KB栈和512B堆；没有测量运行时峰值栈占用。

## 3. 构建和主机检查

在本目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\test-host.ps1
..\..\.venv-pyocd\Scripts\python.exe .\check_image.py
```

build.ps1默认使用 `C:\Keil_v5\ARM\ARMCC\bin`；test-host.ps1使用本机LLVM clang，可指定Clang路径；check_image.py使用已配置的pyelftools，仅检查产物，不连接设备。

历史结果为139项原生协议/心跳检查、11组HAL替身适配场景通过。替身覆盖排队、早到完成回调、BUSY、溢出、优先级和停发；不能证明真实HAL寄存器/IRQ/ACK。ARM构建使用真实vendor HAL。详细记录见[ARM构建报告](../../artifacts/can-arm-build-2026-09-24/README.md)。

## 4. 已下载产物与校验

| 产物 | 已验收版本 |
|---|---|
| BIN | 7044字节；SHA256 `231407bd9f23191dc61f92862b31c3241791c1bdfcd7c4599b46c537a6503b1e` |
| ROM / 静态RAM | 7044 / 2232字节；静态RAM不等于运行时峰值 |
| 文件 | build/canbench.bin、canbench.hex、canbench.axf |
| 元数据 | [artifact-sha256.json](build/artifact-sha256.json) |
| 写入范围 | 首8192字节、4个2KB扇区；镜像之后1148字节由可信备份保留 |
| 校验 | 全512KB Flash读回、范围外数据与选项字节比对 |

下载及恢复过程见[下载报告](../../artifacts/can-flash-2026-09-25/README.md)。早期program-result.json因WFI/SLEEPING判定返回false，原件保留；后续独立运行核验及物理通信证明成功，不能把这个历史false改成true。

日常演示无需再次下载。`tools/program_can_baseline.py`限定镜像哈希、设备身份、新可信完整备份和窄范围写入；LED下载器不能代替它。旧输出目录不可直接重用。后续修正过下载器的运行状态判定，但修正版未再执行一次Flash写入，不能说其写入流程重新验收过。

## 5. 调试入口

[main.c](main.c)负责时钟、LED和主循环；[can_app.c](can_app.c)负责CAN/HAL适配。详细调用关系见[HAL集成说明](../HAL_INTEGRATION.md)。

- `canbench_boot_stage`：1时钟配置、2CAN配置、3主循环；0xE1时钟失败、0xE2 CAN初始化失败、0xEF HardFault。
- `canbench_diag`：收帧/丢弃/格式拒绝、发送完成、BUSY重试、HAL错误码和fault。
- fault：0正常；1初始化/过滤器失败；2首次接收使能失败；3发送提交失败；4CAN错误或后续接收重启失败；5发送超时。

读到SLEEPING时结合心跳与短暂停核快照判断，不单凭CPU状态重刷。DAP操作会影响实时运行，调试快照的短暂停顿应与测试记录一起解释。

# 最终报告落盘一致性修复（2026-09-26）

这是自动化测试报告保存路径的故障注入验证，不是新的硬件验收。三个测试套件均使用内存中的假传输，不访问 CANable、不发送真实 CAN 报文、不修改开发板。

## 问题与修复

- NormalRun / ContinuousRun：原来 `result.md` 写失败即返回，仍可写的 `result.json` 留在启动快照，丢失本轮最终步骤/样本与错误。现在即使 Markdown 失败，也尝试将最终结果和具体 `report_error` 写入 JSON。
- RecoveryRun：原来在写 JSON 后才设置 Markdown 保存错误，磁盘 JSON 缺少该原因。现在先设置错误再落盘。
- 三者均要求两份报告都成功才置 `report_saved=true`、向界面报告完整保存成功；若两份都失败，错误字符串保留两个文件的错误。
- `protocol_passed` 继续表示协议检查本身的结果，与报告完整保存成功分开。当协议通过但报告失败时，`finished` 的通过参数仍为 `false`。

## 红绿证据

基线：`5d66d1baca7635f1d9f46a3ec1ee43d93612aaae`。Ubuntu 20.04 / Qt 5.12.8；独立目录构建，未改动运行中的项目。

| 套件 | 旧实现加新断言（红） | 修复后（绿） |
|---|---|---|
| NormalTest | 14 通过 / 1 失败，`prerequisitesAndWriteFailure` 的磁盘 JSON 与内存报告不一致 | 16 通过 / 0 失败 |
| ContinuousTest | 13 通过 / 1 失败，`gatesAndReportFailure` 的磁盘 JSON 与内存报告不一致 | 15 通过 / 0 失败 |
| RecoveryTest | 12 通过 / 1 失败，`reportFailure` 的磁盘 JSON 缺少错误原因 | 14 通过 / 0 失败 |

计数包含 QtTest 的初始化和清理项。红阶段已经包含 JSON 单独失败的保护用例；绿阶段额外覆盖 Markdown 与 JSON 同时失败，因此每套件多一个数据行。

- [正常回归红日志](red/normal-test.txt) / [绿日志](green/normal-test.txt)
- [持续查询红日志](red/continuous-test.txt) / [绿日志](green/continuous-test.txt)
- [节点恢复红日志](red/recovery-test.txt) / [绿日志](green/recovery-test.txt)
- 构建输出与测试日志同目录；[源码摘要](source-manifest.json)记录每阶段源码与最终六个文件的 SHA-256。

## 最小复现方法

启动套件后，在其新报告目录中创建名为 `result.md` 的目录，使 Markdown 写入确定失败但 JSON 仍可写。完成或取消该轮后，从磁盘读取 `result.json`，与 `run.report()` 完整比较，并检查 `finished(false, ...)` 及错误中包含 `result.md`。

JSON 单独失败/同时失败用例将已有启动 JSON 文件替换为同名目录。它们检查内存 `report_saved=false`、界面完成信号不报通过以及具体失败文件名；两份都失败时必须同时保留两个错误。

## 验证边界

这批独立红绿运行使用基线 DiagnosticEngine，以隔离报告修复；与发送层日志失败修复的最终集成由后续统一测试确认。所有旧报告与真机证据保持原样。若 JSON 路径本身不可写，就无法承诺将失败原因再写进同一 JSON；此时内存报告和界面仍明确保存失败，已有文件不作为本轮完整结果。没有增加自动重试或改变命令取消行为。

公开日志仅替换测试工作区的个人路径，未修改通过/失败行、数量或耗时。原始日志与两阶段源快照保留于本机非仓库目录。

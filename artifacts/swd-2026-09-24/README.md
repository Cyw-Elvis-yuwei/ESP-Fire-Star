# SWD芯片识别与LED编译记录

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

后续状态：本文件保留识别/构建阶段的历史边界。随后已完成备份、LED下载、完整读回和用户绿色常亮确认，见 [LED真机验收记录](../led-flash-2026-09-24/README.md)。

日期：2026-09-24。用户使用原厂配套SWD排线连接DAP与开发板后，要求“开始吧”。本阶段进行芯片身份读取并准备LED产物，未执行程序下载、擦除或复位命令。

## 实际设备结果

使用项目独立Python环境中的pyOCD 0.45.1，通过已枚举的 `X893 ARM CMSIS-DAP HID`（探针ID `DAP_ID_REDACTED`）访问SWD。此名称是设备上报字符串，不作为商业型号鉴定。

- 100 kHz SWD、generic cortex_m目标、attach连接。
- 显式关闭auto_unlock、resume_on_disconnect、CMSIS-Pack debug sequences和内存读取缓存。
- 只请求三个身份寄存器的读取；没有load、flash、erase或reset命令。
- 进程退出码0。

| 地址 | 读出值 | 解释 |
|---|---|---|
| 0xE0042000 | 0x10036414 | DBGMCU_IDCODE：DEV_ID=0x414，高密度STM32F1组；REV_ID=0x1003 |
| 0xE000ED00 | 0x411FC231 | Cortex-M3 CPUID |
| 0x1FFFF7E0（16位） | 0x0200 | Flash容量512 KB |

结合已购板型和实物芯片标记，这些结果与STM32F103ZET6规格一致。单凭ID与容量不能鉴定封装、完整料号或芯片真伪。

结论：DAP与目标MCU的SWD通路实际可用。先前“SWD尚未识别”状态已更新；物理LED、串口数据及CAN收发仍待验证。

证据：[原始读取日志](identity-attach.log)、[完整命令与退出码](identity-attach-command.json)。

参考：[ST RM0008](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)、[pyOCD连接选项](https://pyocd.io/docs/options.html)。

## LED产物实际构建

在 `firmware/led-baseline/` 复用厂家固定提交的寄存器LED示例。实际调用本机ARM Compiler 5.05编译、汇编、链接和转换，未依赖Keil GUI或升级原有工具。

- 预期行为：PB0输出低电平，绿灯常亮；此行为仅经源码检查，尚未下载验证。
- 本次ARM构建退出码0，产物为 `led.axf`、`led.hex`、`led.bin`。
- BIN大小692字节；HEX地址范围0x08000000–0x080002B3。
- 初始SP=0x20000660，复位向量=0x080001B1；栈位于64 KB RAM范围，复位向量Thumb位及地址检查通过。
- HEX与BIN字节一致；BIN SHA256=`d659ad7df99108cf90f1b5005c441315b53cfa3c67c3b10e37ebaa704f22716a`。
- 第一次构建的末尾校验步骤因Windows PowerShell未找到Get-FileHash而失败；改用.NET SHA256后完整构建成功。初次日志保留，不将失败退出码当作通过。

证据：[最终构建日志](led-build-final.log)、[最终构建命令](led-build-final-command.json)、[产物结构检查](led-image-check.json)、[首次构建日志](led-build.log)。

本次仅验证独立LED例程的ARM构建。项目CAN固件业务层尚未接入可烧录的HAL工程，也未完成ARM构建或物理CAN联调。

## 当时建议的下一阶段（历史）

保持当前已成功的SWD接线。进入下载阶段时先保存现有Flash备份，再按实际目标配置下载独立LED产物；由用户观察绿灯并记录结果。当前没有任何程序写入或下载成功证据。

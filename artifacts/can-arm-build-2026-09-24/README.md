# CAN ARM候选构建与离线验证

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

日期：2026-09-24。背景：用户表示螺丝刀和UT136B+都要明天到货，询问现在能做什么。阶段范围为已有CAN业务接入厂家HAL并完成ARM构建，保护已烧录的LED基线与Flash备份。本轮没有连接探针、擦写设备、发送物理CAN帧或改变虚拟机配置。

## 实际结果

| 检查 | 结果 |
|---|---|
| 复用来源 | 固定厂家提交#39，51个原文件Git blob/SHA256校验，原字节未修改 |
| ARM编译 | 本机ARM Compiler5.05，Cortex-M3/C99/O1、STM32F103xE，退出0，最终日志无编译警告/错误 |
| 映像 | BIN7044字节，0x08000000起；HEX/BIN内容一致；初始SP=0x200008b8 |
| 静态资源 | ROM7044字节、RW+ZI2232字节；包含启动文件栈/堆保留，不代表运行时峰值 |
| 向量核对 | Reset、HardFault、SysTick及CAN TX/RX0/SCE均指向ELF中对应实现，互不共用默认地址 |
| 原有业务检查 | Windows clang原生编译运行，139项通过 |
| 新适配检查 | HAL替身驱动实际can_app.c，11组主机场景通过 |
| 真机 | 未烧录，未测量电阻，未验证物理CAN |

BIN SHA256：`231407bd9f23191dc61f92862b31c3241791c1bdfcd7c4599b46c537a6503b1e`。

## 命令与原始记录

工作目录：`$LOCAL_PROJECT`。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File firmware\stm32-can\build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File firmware\stm32-can\test-host.ps1
.\.venv-pyocd\Scripts\python.exe firmware\stm32-can\check_image.py
```

- [最终ARM构建日志](build-final.log)：退出0。
- [主机测试日志](host-tests.log)：构建和运行均退出0，协议139项与适配11组。
- [镜像检查日志](image-check.log)：退出0，含向量地址和文件数。
- [固件工程说明](../../firmware/stm32-can/README.md)、[产物哈希](../../firmware/stm32-can/build/artifact-sha256.json)、[映像结构结果](../../firmware/stm32-can/build/image-check.json)。

前两次构建分别缺少厂家dma_ex和can_ex扩展头文件，是选取供应商依赖时漏项；根据头文件引用补齐并验证原始哈希后构建成功。保留attempt1/2日志，不将它们写成成功构建。

最终静态核对发现厂家SystemInit保持HSI复位时钟，但SystemCoreClock变量默认72MHz。适配main在HAL_Init之前调用SystemCoreClockUpdate，使首次SysTick配置采用实际寄存器推导时钟；后续HAL切换到PLL时会重新设置时基。修正后重新ARM构建、映像检查通过。启动时序尚未真机验证。最终来源与工具版本快照见 [build-inputs.json](build-inputs.json)。

## 检查覆盖与边界

11组主机场景：初始化过滤器/首心跳边界；帧副本与LED/回复顺序/单在途；BUSY保留；提前完成回调；RX溢出计数和回复背压；回复优先/心跳覆盖；无效参数及DLC；错误锁存/取消及停止提交；250ms超时；RX重启/TX提交/初始化失败；原PRIMASK恢复。

这些是主机模拟HAL调用结果下的应用逻辑测试，不能证明真实STM32中断时序或HAL外设运行。最终HEX的向量与vendor哈希核对属于静态映像检查，不是真机运行证据。

首次硬件候选遇到CAN错误会红灯锁存并停止发送，之后需要用户修复连接再按RESET。未增加自动故障恢复、Bootloader或升级功能。编译器/固件软件证据与后续接线、烧录、收发验收分开记录。

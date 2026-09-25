# 独立LED硬件基线

> 历史平台基线：此程序在2026-09-24完成点灯验收；后续已由[CAN固件](../stm32-can/README.md)替换。下文构建/验收事实对应当时阶段，不表示当前板上仍运行本程序。

用途：在接入CAN固件前验证这块STM32F103ZE开发板的下载和PB0绿灯。2026-09-24已经ARM编译、备份后下载、完成全Flash读回，并由用户确认三色LED绿色常亮。验收证据与原Flash备份在 `../../artifacts/led-flash-2026-09-24/README.md`。

## 来源与最小调整

四个厂家原文件来自 `Embedfire-stm32f103-badao/ebf_stm32f103_badao_hal_code` 的固定提交 `6afb695f68d33c2a4e84b1cc1147fd8a422912f6`，目录 `7-使用寄存器点亮LED灯`。精确下载URL和原始SHA256见 `UPSTREAM.json`；原文件保持原始字节和版权声明。

- `main.c`：打开GPIOB时钟，PB0配置为推挽输出并拉低；SystemInit为空。
- `stm32f10x.h`：厂家最小寄存器定义，不是完整CMSIS器件头。
- `startup_stm32f10x_hd.s`：厂家ST启动文件，保持原声明。
- `BH-F103.uvprojx`：保留为来源参考，原设置指定AC5 5.06与DFP2.1.0，未验证在本机Keil GUI直接打开构建。
- 新增 `led.sct` 与 `build.ps1`：使用本机实际存在的ARM Compiler 5.05和明确的512 KB Flash/64 KB RAM地址范围完成命令行构建。

为保留厂家的原样基线，本次使用-O0。厂家最小头文件的寄存器指针没有volatile；后续若修改程序或优化设置，应先补齐寄存器访问语义。此例只用于上电复位后的独立点灯，不复用为CAN固件的时钟/HAL实现。

## 本机已验证构建入口

```powershell
powershell.exe -NoProfile -File "$LOCAL_PROJECT\firmware\led-baseline\build.ps1"
```

默认使用 `C:\Keil_v5\ARM\ARMCC\bin`。构建输出在本目录的 `build/`，脚本不会连接或写入设备。

2026-09-24构建结果：692字节BIN，HEX范围0x08000000–0x080002B3；具体日志、向量检查及SHA256见 `../../artifacts/swd-2026-09-24/README.md` 和 `build/artifact-sha256.json`。

## 与CAN业务层的边界

本例是独立的最小平台验证程序。现有CAN接入说明依赖厂家#39的旧HAL1.1.1；同仓库#12 HAL LED例程使用HAL1.1.4且静态发现重复SystemClock_Config定义，因此本阶段选择依赖更少的#7。后续不将LED例程的HAL目录直接混入CAN工程。

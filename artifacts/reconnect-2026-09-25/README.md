# 接线后USB/SWD只读检查

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-25，用户报告三根CAN线接好、断电测得59.4～59.5Ω，恢复USB连接后要求助手检查。

本轮现场查询结果：

- Windows：CMSIS-DAP USB/HID及CDC COM8均Status OK；开发板CH340 COM10 Status OK。
- SWD：复用此前generic cortex_m、100kHz、attach、auto_unlock=false的只读命令，读得DBGMCU_IDCODE=0x10036414、CPUID=0x411fc231、Flash容量512KB。Core状态Running。
- 启动向量：SP=0x20000660、Reset=0x080001b1，与此前LED映像的向量一致。只比较向量，未重新读取或校验整幅固件。
- VMware：vmrun list返回Total running VMs: 0，CIM查询无vmware-vmx进程。原Ubuntu地址LAN_IP_REDACTED的SSH连接超时。
- 当前Windows PresentOnly查询未发现VID16D0/PID117E的CANable；Ubuntu未运行，无法查询其USB归属或CAN接口。原因尚未确认，不能直接判断模块坏了。

本轮只进行设备枚举、SWD寄存器/向量/运行状态读取；没有halt/reset/下载/擦除、启动虚拟机或改写网络。SWD原始输出见[swd-identity.log](swd-identity.log)，命令/时间/退出码见[swd-command.json](swd-command.json)。

下一步用户启动既有Ubuntu虚拟机，确认CANable已接USB，再按VMware设备菜单将CANable连接给Ubuntu。DAP保持Windows。重新核实接口后再进入CAN候选下载准备，不把旧can0状态当作当前事实。

## 后续定位：CANable进入DFU

用户反馈VMware可移动设备没有CANable。再次WindowsPresentOnly查询发现 `DFU in FS Mode`、VID0483/PIDDF11、序列号CANABLE_SERIAL_REDACTED，与之前CANable正常模式16D0/117E的序列号相同。原COM11记录为非当前的Unknown；DFU实例ProblemCode28。原始属性见[canable-dfu-properties.json](canable-dfu-properties.json)。这足以识别同一适配器当前处于DFU升级模式，但不能直接归因于用户误按BOOT、固件损坏或特定硬件故障。

此时vmrun已显示原Ubuntu虚拟机运行，SSH为connection closed，与前次虚拟机未启动状态不同。当前先处理CANable工作模式：请用户拔其USB、等5秒，插回时不要按住任何小按钮，120Ω开关保持ON。尚未执行刷写、装DFU驱动或拆壳；若仍为DFU再核对BOOT实际位置与状态，不盲目重复插拔。

## 后续：重新插接后恢复正常模式并进入Ubuntu

用户问“现在呢”后现场复核：Windows已无该序列号的DFU设备，Ubuntu SSH恢复，lsusb出现16d0:117e，稳定串口链接含原序列号CANABLE_SERIAL_REDACTED并指向ttyACM0，证明正常模式的CANable已透传Ubuntu。lsusb所显示的人类可读名称含VMware Virtual USB Mouse，不能据此误认鼠标；本次按VID/PID、串口描述及序列号确认身份。

当前ip -br link只有lo、ens33、ens36，尚无can0，pgrep未发现slcand。重新插接恢复成功，没有刷固件或安装驱动；进入DFU的最初触发原因仍未确认。下一步重新创建并启用SLCAN接口，用户需在Ubuntu本地完成sudo步骤。

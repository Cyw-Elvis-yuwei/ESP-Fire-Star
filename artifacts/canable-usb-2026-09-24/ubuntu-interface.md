# Ubuntu CANable接口启动证据

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-24，用户通过VMware将CANable交给Ubuntu20.04虚拟机，助手通过已有SSH连接只读核实：

- VID/PID=16d0:117e；稳定串口链接指向ttyACM0，设备描述CANable2 b158aa7。
- slcand与slcan内核模块已存在。
- 用户执行 `sudo modprobe slcan`、`sudo slcand -o -c -s6 -S 115200 /dev/ttyACM0 can0`。
- 第一次紧接着执行接口UP时提示找不到can0，但随后查询已出现DOWN接口。输出符合守护进程异步创建接口的时序；未重复启动slcand，用户重试UP成功。
- 用户输出与随后SSH查询一致：

```text
4: can0: <NOARP,UP,LOWER_UP> mtu 16 qdisc pfifo_fast state UNKNOWN mode DEFAULT group default qlen 10
    link/can  promiscuity 0 minmtu 0 maxmtu 0 numtxqueues 1 numrxqueues 1 gso_max_size 65536 gso_max_segs 65535
3909 slcand -o -c -s6 -S 115200 /dev/ttyACM0 can0
```

已确认USB透传、SLCAN守护进程及SocketCAN接口启用。参数s6请求500kbps；尚无物理波特率测量或真实CAN收发结果。state UNKNOWN不等同于CAN错误状态，也不能从LOWER_UP推断外部总线、电阻或应答已正常。

下一步为断电后的CAN端子/跳线核对和接线。拔插USB后tty或can接口可能变化，需重新确认再启动已有守护进程，避免重复实例。此文件保存在Windows项目，未同步Ubuntu源码副本。

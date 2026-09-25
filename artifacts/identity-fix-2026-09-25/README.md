# SLCAN被误标为模拟：修复验证

> 公开副本说明：本文件的“原始/原样”描述本地验收档案；此GitHub副本已脱敏个人路径和设备标识，测量值与成功/失败结果保留。图片未改像素。见[发布处理说明](../../publication/README.md)。

> 阶段记录：设备号、进程、窗口状态与“尚未验证”均对应本文件所述当时阶段，不代表现在的待办。2026-09-25初版最终状态见[验收与证据索引](../../docs/验收与证据索引.md)；原始日志、JSON和自动报告保持原样。

2026-09-25。范围为接口分类及显示，不改协议、固件、接线或CLI的仅vcan回归限制。

- core/socketcanidentity.h通过有时限、无shell的ip JSON查询检查类型；未知类型或查询失败拒绝猜测。旧内核SLCAN没有info_kind，link_type为can；vcan/vxcan按显式类型判断，即使改名can0仍属于模拟。
- mainwindow.cpp和connectdialog.cpp同步修正分类和Virtual显示。
- tests/identity.pro及test_socketcanidentity.cpp覆盖12组输入与实际can0；纳入tools/build.sh。普通构建未指定实际接口时跳过现场接口测试。
- [identity-tests.log](identity-tests.log)：Qt5.12.8，15 passed、0 failed、0 skipped（含初始化/清理）。
- [GUI编译日志](build-app-identityfix.log)：编译通过，无warning/error。
- [检查结果](gui-smoke-verification.json)：真实can0只接收15帧心跳，0命令，simulated=false。
- [实际窗口截图](physical-interface-smoke.png)：显示“真实接口，需另行物理验收”及can0；[新报告](corrected-smoke-session/report.json)在关闭时保存，connected/online=false是正常退出状态。
- [部署记录](deployment.json)：原二进制已备份，替换旧GUI进程并启动修复版；不刷写设备，不重做用户按钮操作。

原10命令用户验收报告在[真机验收目录](../physical-gui-2026-09-25/README.md)原样保存。复现分类测试：

```bash
mkdir -p build-identity-tests
cd build-identity-tests
qmake ../tests/identity.pro
make -j2
CANBENCH_TEST_INTERFACE=can0 CANBENCH_TEST_SIMULATED=0 ./canbench-identity-tests
```

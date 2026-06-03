# HTKIS HyperDisk X - Alpha-B 作战计划

## 状态

- Alpha-A: **Passed** (commit 27e09d2, 50轮100%成功率)
- Alpha-B: **Planning** → B1待启动

## 最终目标

Windows Server 2025 / Windows 11 24H2 从 HyperDisk 网络磁盘启动，进入桌面。

验收: Explorer.exe运行, 文件读写正常, 无0x7B

## 总周期: 8周

## Sprint 分解

### Sprint B1: HyperDisk虚拟磁盘可见

目标: WinPE → HyperDiskBus → HyperDiskBlk → Disk.sys → Volume Manager → 出现磁盘

验收:
```cmd
diskpart
list disk
```
出现 Disk 0 HyperDisk Virtual Disk

开发任务:
- driver/bus: IRP_MN_QUERY_DEVICE_RELATIONS 枚举 PDO
- driver/blk: IRP_MJ_READ, IRP_MJ_WRITE, IRP_MJ_DEVICE_CONTROL (512B/4K Sector)
- tools: hd_diskdiag.exe (查询磁盘/卷/驱动)

### Sprint B2: BootMgr读取BCD

目标: BootMgr 能够读取 \Boot\BCD

验收: 串口输出 BCD OPEN SUCCESS

开发任务:
- image_server: 随机块读取 ReadBlockRequest/ReadBlockResponse, P99 < 5ms

### Sprint B3: Winload加载

目标: winload.efi 执行

验收: COM1 输出 WINLOAD START

开发任务:
- client/common/BootTrace: TSC+阶段+状态+错误码, JSON格式

### Sprint B4: BootStart驱动 (最危险阶段)

目标: 加载 HyperDiskBus.sys, HyperDiskBlk.sys, HyperNet.sys

验收: COM1 输出 BUS OK / BLK OK / NET OK

开发任务:
- HyperDiskBus: DriverEntry + AddDevice + StartDevice
- HyperDiskBlk: IRP_MJ_SCSI (READ10/WRITE10/INQUIRY)
- HyperNet: TCP Client + Reconnect + Heartbeat, 启动阶段连接成功率 >99%

### Sprint B5: 消灭0x7B

目标: ntoskrnl → IoInitSystem → MountBootVolume 成功

验收: 看到 Windows Logo (即使随后蓝屏也算B5通过, 因为存储链已建立)

## P0工程: COM1全链路日志

必须实现 HdComTrace():
```cpp
HD_TRACE(MODULE_BLK, "ReadSector=%llu", sector);
```
输出标签: [BUS] [BLK] [NET] [KERNEL]

0x7B出现后必须能定位, 否则B5无法通过。

## 冻结清单

以下模块在Alpha-B期间禁止开发:
- SmartCache (已有代码不动)
- AI Engine
- DNA Service
- Update Service
- Grafana
- Web UI

原因: 桌面还没出来, 这些没有价值

## Alpha-B 验收门槛

| Sprint | 验收 |
|--------|------|
| B1 | Disk Visible |
| B2 | BCD Read |
| B3 | Winload Start |
| B4 | Driver Loaded |
| B5 | Windows Logo |

## 核心原则

Alpha-B禁止扩展功能。目标只有一个:

**让Windows认为 HyperDisk 是一块真实启动磁盘。**

所有开发资源聚焦: HyperDiskBus, HyperDiskBlk, HyperNet, BootTrace, COM1

任何与0x7B无关的需求延期至Alpha-C。

## 关键发现 (Alpha-A)

- WIM Boot Index=2 (之前一直注入Image 1, 修正后文件才生效)
- WinPE无ole32.dll(COM), 无MSVCP140.dll, 需静态CRT
- wimboot虚拟文件系统只映射/Windows/下的文件
- 3源DHCP环境 (路由器+WDS+HyperDisk), Dell可能从路由器拿IP
- Dell MAC: 50:9a:4c:27:3b:85, IP: 192.168.2.8 (静态)
- Telemetry走Gateway:8080, boot.meta走nginx:80 (不同端口)
- Gateway Down时Telemetry静默丢失 (BlackBox v2需POST上传)

## Alpha-A 验收数据

- 50轮连续启动: 100%成功率
- avg_duration_ms: 17
- min_duration_ms: 15
- max_duration_ms: 19
- machine_id稳定: 50:9A:4C:27:3B:85

Chaos Test:
- C01 Gateway Down: BootAgent继续, Telemetry静默丢失
- C02 Nginx Down: iPXE Connection reset, 10s定位
- C03 dnsmasq Down: BIOS层阻断, 10s定位
- 故障定位 ≤ 5min: PASS

# HTKIS HyperDisk X — 使用说明

> 版本：0.1.0 Alpha | 更新日期：2026-05-30

---

## 目录

1. [使用概述](#1-使用概述)
2. [管理控制台](#2-管理控制台)
3. [镜像管理](#3-镜像管理)
4. [终端管理](#4-终端管理)
5. [DNA分组与驱动管理](#5-dna分组与驱动管理)
6. [快照管理](#6-快照管理)
7. [灰度发布策略](#7-灰度发布策略)
8. [客户端启动流程](#8-客户端启动流程)
9. [bootdiag启动诊断](#9-bootdiag启动诊断)
10. [Boot Replay录制与分析](#10-boot-replay录制与分析)
11. [SmartCache缓存调优](#11-smartcache缓存调优)
12. [断网恢复机制](#12-断网恢复机制)
13. [三网VLAN配置](#13-三网vlan配置)
14. [WireShark协议分析](#14-wireshark协议分析)
15. [API参考](#15-api参考)

---

## 1. 使用概述

HTKIS HyperDisk X 的日常使用围绕以下核心工作流：

```
镜像制作 → 镜像上传 → 镜像分发 → 终端启动 → 监控运维
```

### 角色说明

| 角色 | 职责 | 使用工具 |
|------|------|---------|
| 系统管理员 | 镜像管理、终端管理、服务运维 | 管理控制台+API |
| 运维工程师 | 监控告警、故障排查、扩容 | Grafana+日志+SSH |
| 镜像工程师 | 镜像制作、驱动适配、DNA分类 | WinPE+DISM+bootdiag |

---

## 2. 管理控制台

### 2.1 访问控制台

Alpha阶段通过Nginx反向代理访问：

```
http://192.168.2.80/
```

生产环境使用三网隔离，仅管理网可访问：

```
http://10.10.300.10/  (管理网VLAN 300)
```

### 2.2 登录

首次使用需要创建管理员账户：

```bash
# 通过API创建初始管理员
curl -X POST http://192.168.2.80/api/v1/auth/setup \
  -H "Content-Type: application/json" \
  -d '{
    "username": "admin",
    "password": "YOUR_STRONG_PASSWORD",
    "email": "admin@example.com"
  }'
```

之后使用JWT认证：

```bash
# 登录获取token
TOKEN=$(curl -s -X POST http://192.168.2.80/api/v1/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"YOUR_STRONG_PASSWORD"}' \
  | jq -r '.token')

# 使用token访问API
curl -H "Authorization: Bearer $TOKEN" http://192.168.2.80/api/v1/images
```

---

## 3. 镜像管理

### 3.1 镜像概述

HyperDisk X 镜像采用**三层分离**架构：

```
┌──────────────────────────┐
│   App Layer (应用层)      │  — 用户安装的应用/游戏
│   可读写，差异最大         │
├──────────────────────────┤
│   Driver Layer (驱动层)  │  — 硬件驱动+DNA分组
│   按DNA分组共享            │
├──────────────────────────┤
│   OS Layer (系统层)       │  — Windows基础系统
│   所有终端共享，只读       │
└──────────────────────────┘
```

### 3.2 上传镜像

```bash
# 上传WIM镜像文件到服务器
scp my_image.wim debian@192.168.2.80:/opt/hyperdisk/data/images/

# 通过API注册镜像
curl -X POST http://192.168.2.80/api/v1/images \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "win11-gaming-v1",
    "total_size": 21474836480,
    "block_count": 5242880,
    "os_layer": {
      "source": "win11_base.wim",
      "total_size": 8589934592
    },
    "driver_layer": {
      "source": "drivers_optiplex.wim",
      "total_size": 2147483648
    },
    "app_layer": {
      "source": "apps_gaming.wim",
      "total_size": 10737418240
    }
  }'
```

### 3.3 查看镜像列表

```bash
# 列出所有镜像
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/images

# 查看特定镜像详情
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/images/{image_id}

# 响应示例
{
  "images": [
    {
      "image_id": 1,
      "name": "win11-gaming-v1",
      "total_size": 21474836480,
      "block_count": 5242880,
      "os_layer_id": 1,
      "driver_layer_id": 2,
      "app_layer_id": 3,
      "created_at": "2026-05-30T10:00:00Z",
      "updated_at": "2026-05-30T10:00:00Z"
    }
  ]
}
```

### 3.4 删除镜像

```bash
curl -X DELETE -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/images/{image_id}
```

> **注意**：删除镜像前必须确保没有终端正在使用该镜像。系统会检查引用计数。

### 3.5 镜像更新

镜像更新采用**增量更新**方式，只传输变化的块：

```bash
# 创建增量更新包
# 在Windows镜像工作站上：
hd_update_pack.exe --base win11-gaming-v1 --new win11-gaming-v2 --output delta_v1_v2.hdx

# 上传增量包
scp delta_v1_v2.hdx debian@192.168.2.80:/opt/hyperdisk/data/updates/

# 通过API注册更新
curl -X POST http://192.168.2.80/api/v1/images/{image_id}/updates \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "version": "v2",
    "delta_file": "delta_v1_v2.hdx",
    "description": "新增Steam+ Epic Games"
  }'
```

---

## 4. 终端管理

### 4.1 终端注册

终端首次PXE启动时自动注册到MetadataCenter：

```bash
# 查看已注册终端
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/terminals

# 响应示例
{
  "terminals": [
    {
      "terminal_id": 1,
      "hostname": "PC-A01",
      "dna_group_id": 1,
      "status": "online",
      "last_heartbeat": "2026-05-30T10:30:00Z",
      "assigned_image": "win11-gaming-v1"
    }
  ]
}
```

### 4.2 终端状态

| 状态 | 说明 |
|------|------|
| `offline` | 终端关机或断网 |
| `booting` | 正在PXE启动中 |
| `online` | 正常运行中，心跳正常 |
| `degraded` | 断网运行中（RamOverlay模式） |
| `recovering` | 断网恢复中 |

### 4.3 为终端分配镜像

```bash
# 分配镜像给终端
curl -X PUT -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"image_name": "win11-gaming-v1"}' \
  http://192.168.2.80/api/v1/terminals/{terminal_id}/image

# 批量分配（按DNA组）
curl -X PUT -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"image_name": "win11-gaming-v1", "dna_group_id": 1}' \
  http://192.168.2.80/api/v1/terminals/batch-assign
```

### 4.4 终端重启/关机

```bash
# 重启终端（下次启动使用新镜像）
curl -X POST -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/terminals/{terminal_id}/reboot

# 批量重启（如网吧打烊后批量关机）
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"terminal_ids": [1,2,3,4,5], "action": "shutdown"}' \
  http://192.168.2.80/api/v1/terminals/batch-action
```

---

## 5. DNA分组与驱动管理

### 5.1 DNA指纹原理

DNA (Driver Node Architecture) 是对终端硬件驱动集的哈希指纹。相同DNA的终端共享同一驱动层：

```
终端硬件扫描 → 驱动列表排序 → SHA-256哈希 → DNA指纹
```

### 5.2 查看DNA分组

```bash
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/dna-groups

# 响应示例
{
  "dna_groups": [
    {
      "dna_group_id": 1,
      "dna_digest": "a3f2b8c...",
      "driver_layer_id": 2,
      "terminal_count": 45,
      "description": "Dell OptiPlex 3080 (I219-V + UHD630)"
    },
    {
      "dna_group_id": 2,
      "dna_digest": "7e1d9f4...",
      "driver_layer_id": 3,
      "terminal_count": 30,
      "description": "Dell OptiPlex 7090 (I225-V + UHD750)"
    }
  ]
}
```

### 5.3 手动创建DNA分组

```bash
# 通常DNA分组是自动的，但也可以手动创建
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "dna_digest": "a3f2b8c...",
    "driver_layer_id": 2
  }' \
  http://192.168.2.80/api/v1/dna-groups
```

---

## 6. 快照管理

### 6.1 创建快照

快照保存当前镜像的完整状态，支持回滚：

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "image_id": 1,
    "name": "pre-update-snapshot",
    "os_layer_ver": 1,
    "driver_layer_ver": 2,
    "app_layer_ver": 3
  }' \
  http://192.168.2.80/api/v1/snapshots
```

### 6.2 列出快照

```bash
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/images/{image_id}/snapshots
```

### 6.3 回滚到快照

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/snapshots/{snapshot_id}/rollback
```

---

## 7. 灰度发布策略

### 7.1 创建灰度策略

灰度发布支持分批推送镜像更新，每批监控故障率：

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "target_image_id": 2,
    "total_batches": 5,
    "fault_threshold": 0.05
  }' \
  http://192.168.2.80/api/v1/canary-strategies
```

### 7.2 灰度策略状态

| 状态 | 说明 |
|------|------|
| `draft` | 草稿，未开始 |
| `running` | 执行中，当前批次监控中 |
| `paused` | 暂停（故障率接近阈值） |
| `completed` | 全部批次完成 |
| `rolled_back` | 故障超限，已回滚 |

### 7.3 灰度流程

```
创建策略 → 开始 → 批次1(10%终端) → 监控 → 批次2(30%) → 监控 → ... → 全量发布
                                    ↓ 故障超5%
                                  暂停 → 人工决策 → 继续或回滚
```

---

## 8. 客户端启动流程

### 8.1 完整启动链

```
UEFI固件
  ↓
PXE ROM → DHCP获取IP → TFTP下载iPXE
  ↓
iPXE → HTTP下载BootAgent (hd_boot_agent.exe)
  ↓
BootAgent → 建立TCP连接 → ChunkStream下载boot.meta
  ↓
boot.meta包含: 镜像ID+块数+偏移+校验和
  ↓
WinPE加载 → bootdiag.exe执行7项诊断
  ↓
bootdiag PASS → BootMgr → winload.exe
  ↓
winload → 加载HyperDisk驱动栈:
  HyperDiskBus.sys → 枚举虚拟磁盘
  HyperDiskBlk.sys → 虚拟块设备
  HyperCache.sys  → 缓存MiniFilter
  HyperOverlay.sys → 写重定向MiniFilter
  HyperNet.sys    → 网络传输
  ↓
ntoskrnl → 桌面
```

### 8.2 boot.meta缓存文件

boot.meta是客户端本地缓存的关键文件，存储在：

```
C:\HyperDisk\boot.meta
```

内容格式：

```ini
[meta]
version=1
image_id=1
block_count=5242880
block_size=4096
image_checksum=sha256:a3f2b8c...

[servers]
primary=10.10.200.10:9090
secondary=10.10.200.11:9090

[cache]
l1_size_mb=2048
l2_path=D:\HyperDisk\cache
l2_size_mb=8192

[overlay]
ram_size_mb=4096
wal_enabled=true
```

### 8.3 启动时间参考

| 阶段 | 时间 | 说明 |
|------|------|------|
| PXE→iPXE | 1-2s | 网络引导 |
| BootAgent下载 | 2-5s | ChunkStream |
| bootmeta解析 | <1s | 本地缓存 |
| WinPE加载 | 5-10s | 内存加载 |
| bootdiag | 3-8s | 7项诊断 |
| Winload→驱动 | 10-20s | 驱动加载 |
| 桌面就绪 | 20-40s | explorer.exe |
| **总计冷启动** | **40-90s** | Alpha目标<120s |

---

## 9. bootdiag启动诊断

### 9.1 7项诊断清单

bootdiag.exe 在 WinPE 阶段执行7项诊断，**全部PASS才允许继续启动**：

| 编号 | 检查项 | 说明 | 失败处理 |
|------|--------|------|---------|
| 1 | GPT验证 | 确认虚拟磁盘为GPT格式 | 阻止启动 |
| 2 | BCD验证 | 确认启动配置数据完整 | 阻止启动 |
| 3 | winload验证 | 确认winload.exe存在且签名有效 | 阻止启动 |
| 4 | 块读取验证 | 读取首1000块验证校验和 | 降级+COM1告警 |
| 5 | 分页IO验证 | 验证分页文件路径可达 | 降级+COM1告警 |
| 6 | Hive验证 | 验证注册表HIVE完整性 | 降级+COM1告警 |
| 7 | NTFS验证 | 验证文件系统元数据一致 | 降级+COM1告警 |

### 9.2 使用bootdiag

```cmd
:: 在WinPE中运行
bootdiag.exe --config C:\HyperDisk\boot.meta

:: 仅运行特定检查
bootdiag.exe --check gpt,bcd,winload

:: 输出到COM1串口
bootdiag.exe --serial COM1 --baud 115200

:: 查看结果
bootdiag.exe --status
:: 输出:
::   [PASS] GPT验证 - 分区表有效
::   [PASS] BCD验证 - 启动配置完整
::   [PASS] winload验证 - 签名有效
::   [PASS] 块读取验证 - 1000/1000块校验通过
::   [PASS] 分页IO验证 - 路径可达
::   [PASS] Hive验证 - 注册表完整
::   [PASS] NTFS验证 - 元数据一致
::   ----
::   门控结果: PASS (7/7) - 允许启动
```

### 9.3 诊断失败处理

如果任一关键检查(1-3)失败：
- 设置 boot.meta `boot_fail=1` 标志
- 通过COM1输出完整诊断信息
- **阻止进入正式系统**，停留在WinPE等待人工干预

如果非关键检查(4-7)失败：
- COM1输出告警
- 允许继续启动（降级模式）

---

## 10. Boot Replay录制与分析

### 10.1 启动IO录制

Boot Replay Recorder 记录启动全过程的块IO操作：

- **环形缓冲区**：64MB，位于终端内存
- **记录内容**：时间戳+操作码+块偏移+大小+结果
- **刷盘时机**：启动完成后自动写入 `/HyperDisk/replay/`
- **降级输出**：缓冲区满时通过COM1输出

### 10.2 离线分析

使用 `boot_replay_analyzer` 工具分析录制文件：

```cmd
:: 基本分析
boot_replay_analyzer.exe --input replay_20260530_103000.bin

:: 详细分析+生成报告
boot_replay_analyzer.exe --input replay_20260530_103000.bin --report report.html

:: 对比两次启动（检测0x7B复现）
boot_replay_analyzer.exe --diff replay_good.bin replay_bad.bin
```

分析报告包含：
- IO时间线（按时间排序的所有IO操作）
- 热块统计（最频繁访问的块区间）
- 顺序/随机IO比例
- 0x7B特征检测（特定块访问模式）
- AI预读建议（基于访问模式的预读策略）

### 10.3 录制文件格式

```
Header (32B):
  magic:     "HDREPLAY" (8B)
  version:   uint16
  entry_size: uint16
  entry_count: uint32
  start_time: uint64 (epoch_ms)
  end_time:   uint64 (epoch_ms)
  reserved:   uint32

Entry (24B each):
  timestamp: uint32 (相对起始的微秒)
  opcode:    uint8  (READ/WRITE/FLUSH)
  flags:     uint8
  block_offset: uint64
  size:      uint32
  result:    uint8  (SUCCESS/FAIL/RETRY)
  reserved:  uint8
```

---

## 11. SmartCache缓存调优

### 11.1 三级缓存架构

```
┌────────────────────────────────────┐
│ L1: RAM缓存 (W-TinyLFU)           │
│ 容量: 2-4GB                        │
│ 命中率目标: 85%+                    │
│ 算法: Window(1%) +                 │
│       Probation + Protected(99%)   │
│ 频率估计: CountMinSketch           │
├────────────────────────────────────┤
│ L2: 本地NVMe缓存                   │
│ 容量: 8-32GB                       │
│ 命中率目标: 95%+ (L1+L2累计)       │
│ 格式: 4KB块直接映射                 │
├────────────────────────────────────┤
│ L3: 服务器SSD缓存                  │
│ 容量: 64-256GB                     │
│ 命中率目标: 99%+ (L1+L2+L3累计)    │
│ 多终端共享                         │
└────────────────────────────────────┘
```

### 11.2 W-TinyLFU参数调优

通过boot.meta或注册表配置：

```ini
[cache.l1]
size_mb = 2048          ; L1 RAM缓存大小
window_pct = 1          ; Window区占比(%)
protected_pct = 80      ; Protected区占比(%)
probation_pct = 19      ; Probation区占比(%)
sketch_width = 4        ; CountMinSketch宽度
sketch_depth = 4        ; CountMinSketch深度
```

### 11.3 缓存监控

```bash
# 通过API查看缓存统计
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/terminals/{terminal_id}/cache-stats

# 响应示例
{
  "l1": {
    "hit_rate": 0.87,
    "hits": 1234567,
    "misses": 184321,
    "evictions": 98765,
    "size_mb": 2048
  },
  "l2": {
    "hit_rate": 0.95,
    "hits": 156234,
    "misses": 28087,
    "size_mb": 8192
  },
  "l3": {
    "hit_rate": 0.99,
    "hits": 25432,
    "misses": 655,
    "size_mb": 65536
  }
}
```

---

## 12. 断网恢复机制

### 12.1 状态机

```
Connected ←→ Detecting → Disconnected → Reconnecting → Recovering → Connected
                                                        ↓ (失败)
                                                      Offline
```

### 12.2 各状态行为

| 状态 | 检测方式 | 行为 | 超时 |
|------|---------|------|------|
| Connected | 心跳正常 | 正常读写 | - |
| Detecting | 心跳丢失1次 | 继续IO，标记可能断网 | 5s |
| Disconnected | 心跳超时 | 切换RamOverlay模式 | 5s |
| Reconnecting | 指数退避重连 | 尝试主/备服务器 | 1s→2s→4s→...→30s |
| Recovering | 连接恢复 | 同步断网期间的脏块 | 按数据量 |
| Offline | 重连失败超过阈值 | 仅本地RamOverlay | - |

### 12.3 RamOverlay断网保护

断网后所有写入重定向至RamOverlay：
- 内存中的WAL（Write-Ahead Log）记录所有写操作
- 断网期间的写入**不丢失**
- 恢复连接后自动将脏块同步回服务器
- 内存不足时触发LRU淘汰（最旧的写入被丢弃，COM1告警）

### 12.4 配置

```ini
[network]
heartbeat_interval_ms = 1000     ; 心跳间隔
detect_timeout_ms = 5000         ; 断网检测超时
reconnect_base_ms = 1000         ; 重连基础间隔
reconnect_max_ms = 30000         ; 重连最大间隔
max_reconnect_attempts = 0       ; 0=无限重试

[servers]
primary = 10.10.200.10:9090      ; 主服务器
secondary = 10.10.200.11:9090    ; 备用服务器
```

---

## 13. 三网VLAN配置

### 13.1 VLAN用途

| VLAN ID | 网段 | 用途 | 优先级 |
|---------|------|------|--------|
| 100 | 10.10.100.0/24 | PXE引导网 | 低 |
| 200 | 10.10.200.0/24 | 存储数据网（块IO） | 最高 |
| 300 | 10.10.300.0/24 | 管理网（API+监控） | 中 |

### 13.2 交换机配置

需要管理交换机配置VLAN Trunk：

```
# Cisco示例
interface GigabitEthernet1/0/1
  description HTKIS-Server
  switchport trunk encapsulation dot1q
  switchport mode trunk
  switchport trunk allowed vlan 100,200,300

# 客户端端口（Access模式，PXE网+存储网）
interface range GigabitEthernet1/0/10-20
  description HTKIS-Clients
  switchport mode access
  switchport access vlan 100
  switchport voice vlan 200
```

---

## 14. WireShark协议分析

### 14.1 安装解析器

项目提供自定义WireShark LUA解析器：

```
deploy/wireshark/hdxb_dissector.lua
```

安装方法：

```
# 将LUA文件复制到WireShark插件目录
# Windows: %APPDATA%\Wireshark\plugins\
# Linux: ~/.local/share/wireshark/plugins/

cp deploy/wireshark/hdxb_dissector.lua ~/.local/share/wireshark/plugins/
```

### 14.2 协议帧格式

40B帧头格式：

```
偏移  大小  字段           说明
0     4B    Magic          0x48445842 ("HDXB")
4     2B    Version        协议版本 (0x0001)
6     1B    Opcode         操作码 (Read/Write/Heartbeat等)
7     1B    Flags          标志位
8     4B    RequestID      请求ID（匹配响应）
12    4B    PayloadLen     载荷长度
16    8B    ImageID        镜像ID
24    8B    BlockOffset    块偏移
32    4B    BlockCount     块数量
36    1B    LayerID        层ID (OS/Driver/App)
37    3B    Reserved       保留对齐
```

### 14.3 使用WireShark抓包

```
# 在存储网接口抓包
sudo tcpdump -i enp1s0.200 -w hyperdisk.pcap

# 或在WireShark中过滤
hdlx  # 显示所有HyperDisk X协议帧
hdlx.opcode == 0x01  # 仅显示BlockRead请求
```

---

## 15. API参考

### 15.1 API端点一览

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/api/v1/auth/setup` | 初始管理员设置 |
| POST | `/api/v1/auth/login` | 登录获取JWT |
| GET | `/api/v1/images` | 列出所有镜像 |
| POST | `/api/v1/images` | 创建/注册镜像 |
| GET | `/api/v1/images/{id}` | 镜像详情 |
| DELETE | `/api/v1/images/{id}` | 删除镜像 |
| POST | `/api/v1/images/{id}/updates` | 注册增量更新 |
| GET | `/api/v1/terminals` | 列出所有终端 |
| PUT | `/api/v1/terminals/{id}/image` | 分配镜像 |
| POST | `/api/v1/terminals/{id}/reboot` | 重启终端 |
| GET | `/api/v1/terminals/{id}/cache-stats` | 缓存统计 |
| GET | `/api/v1/dna-groups` | 列出DNA分组 |
| GET | `/api/v1/snapshots` | 列出快照 |
| POST | `/api/v1/snapshots` | 创建快照 |
| POST | `/api/v1/snapshots/{id}/rollback` | 回滚到快照 |
| POST | `/api/v1/canary-strategies` | 创建灰度策略 |
| GET | `/api/v1/canary-strategies/{id}` | 灰度策略状态 |
| GET | `/health` | 健康检查 |
| GET | `/metrics` | Prometheus指标 |

### 15.2 错误码

| HTTP状态码 | 错误码 | 说明 |
|-----------|--------|------|
| 400 | INVALID_REQUEST | 请求参数错误 |
| 401 | UNAUTHORIZED | JWT无效或过期 |
| 403 | FORBIDDEN | 权限不足（RBAC） |
| 404 | NOT_FOUND | 资源不存在 |
| 409 | CONFLICT | 资源冲突（如镜像正在使用） |
| 500 | INTERNAL_ERROR | 服务内部错误 |
| 503 | SERVICE_UNAVAILABLE | 服务不可用 |

---

*使用说明结束。下一步请参阅 [运维说明](operations.md)。*

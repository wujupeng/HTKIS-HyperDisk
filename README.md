<p align="center">
  <img src="HTKIS-HyperDisk.png" alt="HTKIS HyperDisk X" width="256"/>
</p>

<h1 align="center">HTKIS HyperDisk X</h1>

<p align="center">
  <strong>下一代 X86 智能无盘系统架构平台</strong>
</p>

<p align="center">
  面向网吧 · 企业终端 · 培训机房 · 电竞酒店 · 云桌面
</p>

<p align="center">
  <em>稳定优先，性能后置 — 宁可慢，不能坏</em>
</p>

<p align="center">
  <a href="docs/deployment.md">部署教程</a> ·
  <a href="docs/usage.md">用户操作手册</a> ·
  <a href="docs/operations.md">运维手册</a>
</p>

---

## 项目简介

HTKIS HyperDisk X 是一套完整的无盘操作系统平台，终端通过网络从服务器拉取系统镜像并启动，所有写入重定向至内存（RamOverlay），实现零本地存储、秒级启动、集中运维。

### 核心特性

| 特性 | 说明 |
|------|------|
| **网络块设备** | 终端PXE启动→虚拟磁盘mount→完整Windows运行 |
| **RamOverlay** | 所有写入重定向至终端内存，基础层只读，断网保护 |
| **SmartCache** | W-TinyLFU三级缓存（L1 RAM + L2 NVMe + L3 Server SSD） |
| **0x7B恢复** | 三阶段恢复（Retry→Fallback→Recovery），COM1完整诊断 |
| **bootdiag门控** | 7项启动链诊断验证，全PASS才允许进入正式系统 |
| **Boot Replay** | 启动全程IO录制→离线回放→0x7B复现→AI预读训练 |
| **断网恢复** | 5秒检测+指数退避重连+RamOverlay持续写入+自动恢复 |
| **三网VLAN** | PXE网(100)+存储网(200)+管理网(300)，安全隔离 |

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    服务端集群 (Debian 13)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ 元数据中心    │  │ 镜像服务     │  │ API Gateway      │  │
│  │ Rust+RocksDB │  │ C++20+epoll  │  │ Rust+axum        │  │
│  │ gRPC+PG      │  │ +io_uring    │  │ JWT+RBAC         │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│         │                  │                    │             │
│  ┌──────────┐     ┌──────────────┐     ┌──────────────┐    │
│  │PostgreSQL│     │ Nginx+TFTP   │     │Prometheus    │    │
│  │  配置存储 │     │ iPXE引导     │     │+Grafana      │    │
│  └──────────┘     └──────────────┘     └──────────────┘    │
└─────────────────────────────────────────────────────────────┘
                          │ TCP+protobuf (VLAN200)
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  客户端终端 (Dell OptiPlex)                  │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ Windows 启动链:                                      │   │
│  │ PXE→iPXE→BootAgent→WinPE→bootdiag→BootMgr→         │   │
│  │ winload→HDB→HDBK→HDN→ntoskrnl→Desktop              │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │SmartCache│  │OverlayMgr│  │DiskRuntime│  │0x7B恢复  │  │
│  │W-TinyLFU │  │RamOverlay│  │断网状态机 │  │三阶段    │  │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘  │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐    │
│  │boot.meta │  │COM1调试  │  │Boot Replay Recorder  │    │
│  │本地缓存  │  │0x3F8串口 │  │IO录制+离线分析       │    │
│  └──────────┘  └──────────┘  └──────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| **内核驱动** | C + WDK | HyperDiskBus/Blk/Net/Cache/Overlay (5个驱动) |
| **客户端** | C++20 + Win32 API | BootAgent/DiskRuntime/SmartCache/OverlayMgr/QoS |
| **元数据中心** | Rust + RocksDB + gRPC (tonic) | KV存储+配置管理+终端调度 |
| **DNA服务** | Rust + gRPC (tonic) | 硬件指纹收集+驱动匹配+分组 |
| **更新服务** | Rust + gRPC (tonic) | 灰度策略+快照+增量计算 |
| **API Gateway** | Rust + axum | RESTful代理+认证+RBAC+健康检查 |
| **镜像服务** | C++20 + epoll + io_uring | 块IO分发+ChunkStream+心跳 |
| **协议** | TCP + Protobuf | 40B帧头+CRC32C校验 |
| **数据库** | PostgreSQL 17 + RocksDB 9.x | 配置存储+高频KV |
| **监控** | Prometheus + Grafana | 指标采集+仪表盘 |
| **部署** | Ansible + systemd | 自动化部署+服务管理 |

---

## 服务端口

| 服务 | 端口 | 协议 | 说明 |
|------|------|------|------|
| MetadataCenter | 50051 | gRPC | 元数据服务 |
| DNA Service | 50052 | gRPC | DNA指纹服务 |
| Update Service | 50053 | gRPC | 更新管理服务 |
| API Gateway | 8080 | HTTP | RESTful API |
| Nginx | 80/443 | HTTP/HTTPS | 反向代理 |
| PostgreSQL | 5432 | TCP | 数据库 |
| Prometheus | 9090 | HTTP | 指标采集 |
| Grafana | 3000 | HTTP | 监控面板 |

---

## 项目结构

```
HTKIS-HyperDisk/
├── driver/              # Windows内核驱动 (C+WDK)
│   ├── bus/             # HyperDiskBus.sys - 虚拟总线枚举
│   ├── blk/             # HyperDiskBlk.sys - 虚拟块设备
│   ├── cache/           # HyperCache.sys - 缓存MiniFilter
│   ├── overlay/         # HyperOverlay.sys - 写重定向MiniFilter
│   ├── net/             # HyperNet.sys - 网络块传输
│   └── common/          # 共享内核工具 (串口/内存/锁/注册表)
├── client/              # 客户端运行时 (C++20)
│   ├── boot_agent/      # 网络引导+ChunkStream+boot.meta
│   ├── disk_runtime/    # 块IO调度+断网恢复状态机
│   ├── smart_cache/     # W-TinyLFU L1/L2缓存
│   ├── overlay_mgr/     # RamOverlay+WAL增强
│   ├── driver_manager/  # 驱动匹配+DNA指纹
│   ├── update_agent/    # 增量更新+版本管理
│   ├── qos_engine/      # 优先级调度+令牌桶
│   └── common/          # 协议帧编解码+配置+日志+BootTrace
├── server/              # 服务端 (Rust + C++20)
│   ├── metadata_center/ # 元数据中心 (Rust+RocksDB+gRPC)
│   ├── api_gateway/     # API网关 (Rust+axum)
│   ├── dna_service/     # DNA指纹服务 (Rust+gRPC)
│   ├── update_service/  # 更新管理服务 (Rust+gRPC)
│   ├── image_server/    # 镜像服务 (C++20+epoll+io_uring)
│   ├── cache_engine/    # L3 SSD缓存 (C++20)
│   ├── ai_engine/       # AI预读引擎 (C++20)
│   ├── dpdk_gateway/    # DPDK网关 (C++20, Phase2)
│   └── common/          # 共享Rust库 (配置/类型/错误)
├── proto/               # gRPC协议定义 (metadata/dna/update)
├── tools/               # 工具集
│   ├── bootdiag/        # 7项启动诊断 (C+Win32, <128KB)
│   ├── replay_recorder/ # IO录制+离线分析 (C++20)
│   └── mini_image/      # 最小镜像构建 (PowerShell+DISM)
├── config/              # 数据库Schema (PostgreSQL)
├── deploy/              # 部署配置
│   ├── ansible/         # Ansible自动化
│   ├── systemd/         # systemd服务单元
│   ├── docker/          # Docker容器
│   ├── wireshark/       # WireShark LUA解析器
│   └── network/         # 三网VLAN配置
├── tests/               # 测试套件
│   ├── unit/            # 单元测试
│   └── integration/     # 集成测试
└── docs/                # 文档
    ├── deployment.md    # 部署教程
    ├── usage.md         # 用户操作手册
    └── operations.md    # 运维手册
```

---

## 快速开始

### 前置条件

**服务端：**
- Debian 13 (Trixie) 或 Ubuntu 24.04+
- 内核 ≥ 6.1（io_uring完整支持）
- NVMe SSD（镜像存储）
- Intel I219/I225 NIC（万兆网络推荐）
- 8GB+ RAM, 4+ CPU cores

**客户端：**
- Dell OptiPlex 3050/3060/3070/3080/3090/5000/7000
- UEFI固件 + Network Boot支持
- 8GB+ RAM（RamOverlay需要）
- COM1串口（调试可选）

### 编译

#### Windows 客户端 + 工具（MSVC）

```bash
# 需要Visual Studio 2026 + CMake 4.x
cmake -B build -G "Visual Studio 18 2026" -A x64 \
  -DBUILD_CLIENT=ON -DBUILD_TOOLS=ON -DBUILD_TESTS=ON
cmake --build build --config Release

# 产出
#   build/client/boot_agent/Release/hd_boot_agent.exe
#   build/bin/Release/bootdiag.exe
#   build/bin/Release/boot_replay_analyzer.exe
#   7个.lib + 6个测试
```

#### Linux 服务端（Rust）

```bash
# 需要rustc 1.80+ + cargo + protoc
cd server
cargo build --release

# 产出
#   target/release/hd-metadata-center    (gRPC :50051)
#   target/release/hd-api-gateway        (HTTP :8080)
#   target/release/hd-dna-service        (gRPC :50052)
#   target/release/hd-update-service     (gRPC :50053)
```

#### Linux 服务端（C++20）

```bash
# 需要gcc-14+ + cmake 3.24+ + liburing-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SERVER=ON
cmake --build build

# 产出
#   build/server/image_server/image_server
#   build/server/cache_engine/cache_engine
```

### 运行测试

```bash
# Windows
build\tests\unit\Release\hd_test_protocol.exe
build\tests\unit\Release\hd_test_cache.exe
build\tests\unit\Release\hd_test_overlay.exe
build\tests\unit\Release\hd_test_frame_codec.exe
build\tests\integration\Release\hd_test_boot_flow.exe
build\tests\integration\Release\hd_test_block_io.exe

# Linux
cargo test --workspace
```

### 快速部署（单机Alpha）

```bash
# 1. 安装依赖
sudo apt install -y build-essential gcc-14 cmake liburing-dev \
  librocksdb-dev protobuf-compiler libclang-dev \
  postgresql nginx

# 2. 安装Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source ~/.cargo/env

# 3. 编译
cd server && cargo build --release

# 4. 初始化数据库
sudo -u postgres psql -c "CREATE USER hyperdisk WITH PASSWORD 'hyperdisk_2026';"
sudo -u postgres psql -c "CREATE DATABASE hyperdisk OWNER hyperdisk;"
psql -U hyperdisk -d hyperdisk -f config/schema.sql

# 5. 启动服务
./target/release/hd-metadata-center --config etc/metadata.toml &
./target/release/hd-api-gateway --config etc/gateway.toml &
./target/release/hd-dna-service &
./target/release/hd-update-service &

# 6. 验证
curl http://localhost:8080/health
# 期望: {"status":"ok","version":"0.1.0","uptime_seconds":...}
```

---

## Alpha Bring-Up 里程碑

| 里程碑 | 目标 | 验收标准 | 状态 |
|--------|------|---------|------|
| **A** | WinPE网络块设备挂载 | bootdiag 7项全PASS + mount成功 | 🔄 进行中 |
| **B** | BootMgr→ntoskrnl加载 | 0x7B不发生 + BootStart驱动≤15 | ⏳ 待开始 |
| **C** | 桌面可见+文件CRUD | explorer.exe运行 + 5项文件操作成功 | ⏳ 待开始 |
| **D** | 断网30秒不崩溃 | RamOverlay继续写入 + 5秒内重连恢复 | ⏳ 待开始 |
| **E** | 100台并发冷启动 | 5分钟内全部进入桌面 + P95≤120s | ⏳ 待开始 |

### 当前进度

- ✅ 协议帧编解码 (40B帧头+CRC32C) + WireShark LUA解析器
- ✅ W-TinyLFU L1 RAM缓存 (CountMinSketch+三区淘汰)
- ✅ 断网恢复状态机 (6状态+指数退避)
- ✅ bootdiag.exe (7项启动诊断, 26KB)
- ✅ Boot Replay Recorder (64MB环形缓冲区+离线分析)
- ✅ 4个Rust服务 gRPC/HTTP server (已部署运行)
- ✅ PostgreSQL Schema (8表+索引)
- ✅ Nginx 反向代理 + systemd 服务管理

---

## 文档

| 文档 | 说明 |
|------|------|
| [部署教程](docs/deployment.md) | 完整的服务端+客户端部署指南，含系统要求、网络VLAN、手动/Ansible部署、Nginx配置、数据库初始化、排错清单 |
| [用户操作手册](docs/usage.md) | 日常使用指南：镜像管理、终端管理、DNA分组、快照、灰度发布、bootdiag诊断、Boot Replay、SmartCache调优、API参考 |
| [运维手册](docs/operations.md) | 运维指南：服务管理、日志管理、监控告警、性能调优、故障排查、备份恢复、扩容缩容、升级回滚、安全运维、日常巡检、应急响应 |

---

## 许可证

Copyright © 2026 HTKIS. All rights reserved.

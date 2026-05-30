# HTKIS HyperDisk X — 部署说明

> 版本：0.1.0 Alpha | 更新日期：2026-05-30 | 适用系统：Debian 13 (Trixie)

---

## 目录

1. [部署概述](#1-部署概述)
2. [系统要求](#2-系统要求)
3. [网络架构与VLAN规划](#3-网络架构与vlan规划)
4. [服务端部署（完整手动流程）](#4-服务端部署完整手动流程)
5. [服务端部署（Ansible自动化）](#5-服务端部署ansible自动化)
6. [客户端部署](#6-客户端部署)
7. [Nginx反向代理配置](#7-nginx反向代理配置)
8. [数据库初始化](#8-数据库初始化)
9. [服务启动与验证](#9-服务启动与验证)
10. [最小镜像构建](#10-最小镜像构建)
11. [常见部署问题与解决](#11-常见部署问题与解决)
12. [部署检查清单](#12-部署检查清单)

---

## 1. 部署概述

HTKIS HyperDisk X 采用 **客户端-服务端** 分离架构：

- **服务端**：运行在 Debian 13 Linux 上，包含元数据中心、镜像服务、API网关三个核心服务
- **客户端**：运行在 Windows 终端上，通过 PXE 网络启动，从服务端拉取系统镜像

部署顺序：**网络→数据库→Rust服务→C++服务→Nginx→PXE→客户端驱动**

### 服务组件一览

| 组件 | 语言 | 端口 | 依赖 | 说明 |
|------|------|------|------|------|
| MetadataCenter | Rust | 50051 (gRPC) | PostgreSQL, RocksDB | 元数据中心，管理镜像/终端/DNA配置 |
| ImageServer | C++20 | 9090 (TCP) | io_uring, NVMe SSD | 块IO分发服务，核心数据通道 |
| API Gateway | Rust | 8080 (HTTP) | MetadataCenter | RESTful API网关，JWT认证+RBAC |
| Nginx | - | 80/443 | Gateway | 反向代理+静态资源+SSL终端 |
| TFTP/iPXE | - | 69 (UDP) | - | PXE引导服务 |
| PostgreSQL | - | 5432 | - | 配置与关系数据存储 |
| Prometheus | - | 9090 | - | 指标采集 |
| Grafana | - | 3000 | Prometheus | 监控仪表盘 |

---

## 2. 系统要求

### 2.1 服务端硬件要求

| 项目 | 最低要求 | 推荐配置 | 说明 |
|------|---------|---------|------|
| CPU | 4核 x86_64 | 8核+ (Intel Xeon E-2388G) | ImageServer需要CPU亲和性绑定 |
| 内存 | 8GB | 32GB+ DDR4 ECC | RocksDB缓存+镜像缓存 |
| 系统盘 | 50GB SSD | 100GB NVMe SSD | OS+RocksDB+日志 |
| 镜像盘 | 500GB NVMe SSD | 2TB NVMe SSD (Intel P5316) | 镜像存储，必须是NVMe（io_uring） |
| 网络 | 1GbE | 10GbE (Intel I219/I225) | 存储网建议万兆 |
| 串口 | - | COM1 (可选) | 调试输出 |

### 2.2 服务端软件要求

| 软件 | 版本要求 | 安装方式 | 说明 |
|------|---------|---------|------|
| OS | Debian 13 (Trixie) | - | 必须Debian 13+，内核≥6.1 |
| 内核 | ≥ 6.1 | - | io_uring完整支持（6.5+更佳） |
| GCC | ≥ 14.0 | apt | C++20编译 |
| CMake | ≥ 3.24 | apt | 构建系统 |
| Rust | ≥ 1.80 | rustup | 元数据中心+网关编译 |
| liburing | ≥ 2.5 | apt/liburing-dev | io_uring用户态库 |
| RocksDB | ≥ 9.0 | apt/librocksdb-dev | KV存储引擎 |
| PostgreSQL | ≥ 16 | apt | 配置数据库 |
| gRPC/protobuf | - | apt | Rust gRPC依赖 |
| Nginx | ≥ 1.24 | apt | 反向代理 |
| dnsmasq | - | apt | TFTP+DHCP（PXE引导） |

### 2.3 客户端硬件要求

| 项目 | 要求 | 说明 |
|------|------|------|
| 机型 | Dell OptiPlex 3050/3060/3070/3080/3090/5000/7000 | 已验证DNA驱动集 |
| 固件 | UEFI + Network Boot (PXE) | 不支持Legacy BIOS |
| 内存 | ≥ 8GB DDR4 | RamOverlay需要足够内存 |
| 网卡 | Intel I219-V/I219-LM/I225-V | PXE+存储网 |
| 串口 | COM1 (0x3F8, 可选) | 调试诊断输出 |

---

## 3. 网络架构与VLAN规划

### 3.1 三网隔离架构

HTKIS HyperDisk X 使用三个VLAN实现网络隔离：

```
┌─────────────────────────────────────────────┐
│           管理网 VLAN 300 (10.10.300.0/24)    │
│  API Gateway(8080) + Nginx(80/443)           │
│  Prometheus(9090) + Grafana(3000) + SSH(22)  │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│           存储网 VLAN 200 (10.10.200.0/24)    │
│  ImageServer(9090) TCP+protobuf              │
│  高吞吐块IO通道，万兆推荐                       │
└─────────────────────────────────────────────┘

┌─────────────────────────────────────────────┐
│           PXE网 VLAN 100 (10.10.100.0/24)     │
│  DHCP(67) + TFTP(69) + iPXE引导              │
│  终端PXE启动专用通道                            │
└─────────────────────────────────────────────┘
```

### 3.2 VLAN配置脚本

使用项目提供的 `deploy/network/setup-vlans.sh` 脚本配置：

```bash
# 在服务器上执行（需要物理网卡名为enp1s0）
sudo bash deploy/network/setup-vlans.sh enp1s0
```

该脚本将创建：
- `enp1s0.100` — PXE网接口 (10.10.100.10/24)
- `enp1s0.200` — 存储网接口 (10.10.200.10/24)
- `enp1s0.300` — 管理网接口 (10.10.300.10/24)

### 3.3 Alpha阶段简化配置

Alpha Bring-Up阶段可使用**单网**配置（所有流量走同一网段），跳过VLAN设置：

```
单网模式：192.168.2.0/24
- 服务器IP：192.168.2.80
- 所有服务端口均可直接访问
- 仅用于开发测试，生产环境必须三网隔离
```

---

## 4. 服务端部署（完整手动流程）

以下步骤以 Debian 13 服务器（IP: 192.168.2.80）为例，用户名 `debian`。

### 4.1 步骤一：系统更新与基础依赖

```bash
# 更新系统
sudo apt update && sudo apt upgrade -y

# 安装基础工具
sudo apt install -y \
    build-essential gcc-14 g++-14 \
    cmake make pkg-config \
    git curl wget \
    liburing-dev librocksdb-dev \
    libgrpc-dev libprotobuf-dev protobuf-compiler \
    postgresql postgresql-client \
    nginx dnsmasq \
    python3 python3-pip \
    prometheus grafana \
    net-tools ethtool pciutils lshw
```

### 4.2 步骤二：安装Rust工具链

**推荐使用 rustup（比apt快10倍以上）：**

```bash
# 安装rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y

# 加载环境
source ~/.cargo/env

# 设置stable工具链
rustup default stable

# 验证
rustc --version    # 期望: rustc 1.96.0+
cargo --version    # 期望: cargo 1.96.0+
```

**备选方案（不推荐，非常慢）：**

```bash
sudo apt install -y rustc cargo
```

### 4.3 步骤三：创建部署目录与用户

```bash
# 创建系统用户
sudo useradd -r -s /sbin/nologin -d /opt/hyperdisk hyperdisk

# 创建目录结构
sudo mkdir -p /opt/hyperdisk/{bin,etc,logs,data,replay,server,proto,config}
sudo chown -R hyperdisk:hyperdisk /opt/hyperdisk

# 目录说明：
# /opt/hyperdisk/bin/      — 可执行文件
# /opt/hyperdisk/etc/      — 服务配置文件
# /opt/hyperdisk/logs/     — 运行日志
# /opt/hyperdisk/data/     — 镜像数据存储
# /opt/hyperdisk/replay/   — Boot Replay录制文件
# /opt/hyperdisk/server/   — 源码（编译用）
# /opt/hyperdisk/proto/    — protobuf定义
# /opt/hyperdisk/config/   — 数据库schema等
```

### 4.4 步骤四：上传源码

从开发机上传源码到服务器：

```bash
# 方法1：scp（推荐）
scp -r ./server debian@192.168.2.80:/opt/hyperdisk/server
scp -r ./proto debian@192.168.2.80:/opt/hyperdisk/proto
scp -r ./config debian@192.168.2.80:/opt/hyperdisk/config
scp -r ./deploy debian@192.168.2.80:/opt/hyperdisk/deploy

# 方法2：rsync（增量同步，更快）
rsync -avz --exclude='target' ./server/ debian@192.168.2.80:/opt/hyperdisk/server/
rsync -avz ./proto/ debian@192.168.2.80:/opt/hyperdisk/proto/
rsync -avz ./config/ debian@192.168.2.80:/opt/hyperdisk/config/

# 方法3：git clone（如果有远程仓库）
ssh debian@192.168.2.80
cd /opt/hyperdisk
git clone https://github.com/wujupeng/HTKIS-HyperDisk.git .
```

### 4.5 步骤五：编译Rust服务端

```bash
cd /opt/hyperdisk/server

# Release编译（推荐）
cargo build --release -p metadata_center -p api_gateway

# 编译产出
#   target/release/metadata_center
#   target/release/api_gateway

# 复制到bin目录
cp target/release/metadata_center /opt/hyperdisk/bin/hd-metadata-center
cp target/release/api_gateway /opt/hyperdisk/bin/hd-api-gateway

# 验证
/opt/hyperdisk/bin/hd-metadata-center --version
/opt/hyperdisk/bin/hd-api-gateway --version
```

**编译时间参考：**
- 首次编译（含依赖下载）：15-30分钟
- 增量编译：1-3分钟

**常见编译问题：**

| 错误 | 原因 | 解决 |
|------|------|------|
| `rocksdb` link失败 | librocksdb-dev未装 | `apt install librocksdb-dev` |
| `protoc` not found | protobuf-compiler未装 | `apt install protobuf-compiler` |
| openssl link失败 | libssl-dev未装 | `apt install libssl-dev` |

### 4.6 步骤六：编译C++服务端（ImageServer）

```bash
cd /opt/hyperdisk/server

# C++部分使用CMake
cmake -B build-cpp -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SERVER=ON \
    -DCMAKE_C_COMPILER=gcc-14 \
    -DCMAKE_CXX_COMPILER=g++-14

cmake --build build-cpp -j$(nproc)

# 复制到bin目录
cp build-cpp/server/image_server/image_server /opt/hyperdisk/bin/hd-image-server

# 验证
/opt/hyperdisk/bin/hd-image-server --version
```

### 4.7 步骤七：编译客户端（Windows交叉编译或本机编译）

客户端在 **Windows开发机** 上使用 MSVC 编译：

```powershell
# Visual Studio 2026 + CMake 4.x
cmake -B build -G "Visual Studio 18 2026" -A x64 `
    -DBUILD_CLIENT=ON -DBUILD_TOOLS=ON -DBUILD_TESTS=ON

cmake --build build --config Release

# 产出：
#   build/client/boot_agent/Release/hd_boot_agent.exe
#   build/client/disk_runtime/Release/hd_disk_runtime.lib
#   build/client/smart_cache/Release/hd_smart_cache.lib
#   build/client/overlay_mgr/Release/hd_overlay_mgr.lib
#   build/client/common/Release/hd_common.lib
#   build/bin/Release/bootdiag.exe
#   build/bin/Release/boot_replay_analyzer.exe
```

**内核驱动编译（需要WDK）：**

```powershell
# 需要安装 Windows Driver Kit (WDK) 11
# 驱动使用 WDK 的 build 环境
# 产出5个.sys驱动文件：
#   HyperDiskBus.sys  — 虚拟总线枚举
#   HyperDiskBlk.sys  — 虚拟块设备
#   HyperCache.sys    — 缓存MiniFilter
#   HyperOverlay.sys  — 写重定向MiniFilter
#   HyperNet.sys      — 网络块传输
```

---

## 5. 服务端部署（Ansible自动化）

项目提供 Ansible Playbook 实现一键部署：

### 5.1 前置条件

```bash
# 在管理机上安装Ansible
pip3 install ansible

# 配置主机清单
cat > inventory.yml << 'EOF'
hyperdisk_servers:
  hosts:
    192.168.2.80:
      ansible_user: debian
      ansible_ssh_private_key_file: ~/.ssh/id_rsa
EOF
```

### 5.2 执行部署

```bash
# 完整部署
ansible-playbook -i inventory.yml deploy/ansible/site.yml

# 仅部署环境（不启动服务）
ansible-playbook -i inventory.yml deploy/ansible/site.yml --tags setup

# 仅部署网络
ansible-playbook -i inventory.yml deploy/ansible/site.yml --tags network
```

### 5.3 Playbook角色说明

| 角色 | 说明 | 标签 |
|------|------|------|
| `hd_env` | 系统依赖+Rust+GCC+用户创建 | setup |
| `hd_network` | VLAN+路由+防火墙 | network |
| `hd_database` | PostgreSQL初始化+Schema | database |
| `hd_metadata` | MetadataCenter编译+部署+systemd | metadata |
| `hd_imageserver` | ImageServer编译+部署+systemd | imageserver |
| `hd_gateway` | API Gateway编译+部署+systemd | gateway |
| `hd_monitoring` | Prometheus+Grafana配置 | monitoring |

---

## 6. 客户端部署

### 6.1 UEFI配置

在每台客户端终端的BIOS/UEFI中设置：

1. **启动模式**：UEFI Only（关闭Legacy/CSM）
2. **网络启动**：启用 PXE Boot，设置为首启动项
3. **安全启动**：关闭 Secure Boot（驱动无签名）
4. **串口配置**（可选）：启用COM1, 115200 8N1

### 6.2 驱动安装

客户端驱动需要在 WinPE 阶段通过 boot_agent 自动加载，无需手动安装。但开发调试时可在正式系统中手动安装：

```powershell
# 以管理员权限运行
# 安装总线驱动（必须第一个）
pnputil /add-driver HyperDiskBus.inf /install

# 安装块设备驱动
pnputil /add-driver HyperDiskBlk.inf /install

# 安装MiniFilter驱动
pnputil /add-driver HyperCache.inf /install
pnputil /add-driver HyperOverlay.inf /install

# 安装网络传输驱动
pnputil /add-driver HyperNet.inf /install
```

### 6.3 bootdiag部署

bootdiag.exe 需要嵌入 WinPE 镜像：

```powershell
# 使用DISM注入bootdiag到WinPE
dism /image:C:\WinPE_mount /Add-ProvisioningPackage:/PackagePath:bootdiag.exe
```

---

## 7. Nginx反向代理配置

### 7.1 配置文件

创建 `/etc/nginx/sites-available/hyperdisk`：

```nginx
upstream hd_gateway {
    server 127.0.0.1:8080;
    keepalive 32;
}

server {
    listen 80;
    server_name _;

    # 重定向到HTTPS（生产环境启用）
    # return 301 https://$host$request_uri;

    # Alpha阶段直接HTTP
    location / {
        proxy_pass http://hd_gateway;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;

        # WebSocket支持（用于实时监控）
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";

        # 超时配置
        proxy_connect_timeout 10s;
        proxy_read_timeout 300s;
        proxy_send_timeout 300s;
    }

    # 静态资源（管理控制台前端）
    location /static/ {
        alias /opt/hyperdisk/static/;
        expires 7d;
        add_header Cache-Control "public, immutable";
    }

    # Prometheus指标（仅内网可访问）
    location /metrics {
        allow 10.10.300.0/24;
        allow 127.0.0.1;
        deny all;
        proxy_pass http://127.0.0.1:9090/metrics;
    }

    # 健康检查
    location /health {
        proxy_pass http://hd_gateway/health;
        access_log off;
    }
}

# HTTPS配置（生产环境启用）
# server {
#     listen 443 ssl http2;
#     server_name hyperdisk.example.com;
#
#     ssl_certificate /etc/nginx/ssl/hyperdisk.crt;
#     ssl_certificate_key /etc/nginx/ssl/hyperdisk.key;
#     ssl_protocols TLSv1.3;
#     ssl_ciphers HIGH:!aNULL:!MD5;
#
#     location / {
#         proxy_pass http://hd_gateway;
#         ...同上...
#     }
# }
```

### 7.2 启用配置

```bash
# 创建符号链接
sudo ln -sf /etc/nginx/sites-available/hyperdisk /etc/nginx/sites-enabled/hyperdisk

# 移除默认站点（如果存在）
sudo rm -f /etc/nginx/sites-enabled/default

# 测试配置
sudo nginx -t

# 重载Nginx
sudo systemctl reload nginx
```

---

## 8. 数据库初始化

### 8.1 PostgreSQL配置

```bash
# 启动PostgreSQL
sudo systemctl enable postgresql
sudo systemctl start postgresql

# 创建用户和数据库
sudo -u postgres psql << 'SQL'
CREATE USER hyperdisk WITH PASSWORD 'hyperdisk_2026' ENCRYPTED;
CREATE DATABASE hyperdisk OWNER hyperdisk;
GRANT ALL PRIVILEGES ON DATABASE hyperdisk TO hyperdisk;
SQL
```

### 8.2 初始化Schema

```bash
# 使用项目提供的schema文件
PGPASSWORD=hyperdisk_2026 psql -U hyperdisk -d hyperdisk -f /opt/hyperdisk/config/schema.sql
```

Schema包含8张表：

| 表名 | 说明 | 主键 |
|------|------|------|
| `images` | 镜像表 | image_id (BIGSERIAL) |
| `layers` | 镜像层表 | layer_id (SERIAL) |
| `terminals` | 终端表 | terminal_id (BIGSERIAL) |
| `dna_groups` | DNA分组表 | dna_group_id (SERIAL) |
| `snapshots` | 快照表 | snapshot_id (BIGSERIAL) |
| `canary_strategies` | 灰度策略表 | strategy_id (BIGSERIAL) |
| `server_nodes` | 服务器节点表 | node_id (SERIAL) |
| `tenants` | 租户表 | tenant_id (SERIAL) |

### 8.3 PostgreSQL性能调优

编辑 `/etc/postgresql/17/main/postgresql.conf`：

```ini
# 连接
max_connections = 200
listen_addresses = 'localhost'

# 内存（假设32GB服务器）
shared_buffers = 8GB
effective_cache_size = 24GB
work_mem = 64MB
maintenance_work_mem = 512MB

# WAL
wal_level = replica
max_wal_size = 2GB
min_wal_size = 512MB

# 检查点
checkpoint_completion_target = 0.9

# 查询规划
random_page_cost = 1.1        # SSD
effective_io_concurrency = 200  # SSD

# 日志
log_min_duration_statement = 500  # 慢查询>500ms
log_checkpoints = on
log_connections = on
log_disconnections = on
```

重启生效：`sudo systemctl restart postgresql`

---

## 9. 服务启动与验证

### 9.1 部署systemd服务单元

```bash
# 复制服务单元文件
sudo cp /opt/hyperdisk/deploy/systemd/hyperdisk-metacenter.service /etc/systemd/system/
sudo cp /opt/hyperdisk/deploy/systemd/hyperdisk-gateway.service /etc/systemd/system/
sudo cp /opt/hyperdisk/deploy/systemd/hyperdisk-imageserver.service /etc/systemd/system/

# 重载systemd
sudo systemctl daemon-reload
```

### 9.2 创建服务配置文件

**MetadataCenter配置** `/opt/hyperdisk/etc/metadata.toml`：

```toml
[server]
listen_addr = "0.0.0.0:50051"
grpc_reflection = true

[database]
host = "localhost"
port = 5432
database = "hyperdisk"
user = "hyperdisk"
password = "hyperdisk_2026"
max_connections = 20

[rocksdb]
data_dir = "/opt/hyperdisk/data/rocksdb"
cache_size_mb = 4096

[logging]
level = "info"
format = "json"
output = "/opt/hyperdisk/logs/metadata-center.log"
```

**API Gateway配置** `/opt/hyperdisk/etc/gateway.toml`：

```toml
[server]
listen_addr = "0.0.0.0:8080"

[metadata]
grpc_addr = "http://127.0.0.1:50051"

[auth]
jwt_secret = "CHANGE_THIS_IN_PRODUCTION"
jwt_expiry_hours = 24

[logging]
level = "info"
format = "json"
output = "/opt/hyperdisk/logs/api-gateway.log"
```

**ImageServer配置** `/opt/hyperdisk/etc/imageserver.toml`：

```toml
[server]
listen_addr = "0.0.0.0:9090"
io_uring_entries = 256
io_uring_flags = "SQPOLL"

[storage]
image_dir = "/opt/hyperdisk/data/images"
block_size = 4096
chunk_size = 262144

[cache]
l3_cache_dir = "/opt/hyperdisk/data/l3_cache"
l3_cache_size_gb = 64

[logging]
level = "info"
output = "/opt/hyperdisk/logs/image-server.log"
```

### 9.3 启动服务

```bash
# 启动MetadataCenter（必须第一个启动）
sudo systemctl enable hyperdisk-metacenter
sudo systemctl start hyperdisk-metacenter

# 启动ImageServer
sudo systemctl enable hyperdisk-imageserver
sudo systemctl start hyperdisk-imageserver

# 启动API Gateway
sudo systemctl enable hyperdisk-gateway
sudo systemctl start hyperdisk-gateway
```

### 9.4 验证服务状态

```bash
# 检查服务状态
sudo systemctl status hyperdisk-metacenter
sudo systemctl status hyperdisk-imageserver
sudo systemctl status hyperdisk-gateway

# 检查端口监听
ss -tlnp | grep -E '50051|8080|9090'

# 验证gRPC
grpcurl -plaintext localhost:50051 list

# 验证HTTP
curl http://localhost:8080/health

# 验证Nginx代理
curl http://192.168.2.80/health
```

### 9.5 设置开机自启动

```bash
# 所有服务已通过systemctl enable设置开机自启
# 验证
sudo systemctl is-enabled hyperdisk-metacenter
sudo systemctl is-enabled hyperdisk-imageserver
sudo systemctl is-enabled hyperdisk-gateway
```

---

## 10. 最小镜像构建

使用项目提供的 PowerShell 脚本构建最小WinPE镜像：

```powershell
# 在Windows开发机上执行
.\tools\mini_image\build_mini_image.ps1

# 脚本使用DISM自动：
# 1. 创建WinPE基础镜像
# 2. 注入bootdiag.exe
# 3. 注入hd_boot_agent.exe
# 4. 注入必要的驱动
# 5. 生成WIM/ISO文件
```

构建完成后将镜像上传到服务器：

```bash
# 上传到服务器镜像目录
scp winpe_mini.wim debian@192.168.2.80:/opt/hyperdisk/data/images/

# 在服务器上注册镜像（通过API）
curl -X POST http://192.168.2.80/api/v1/images \
  -H "Content-Type: application/json" \
  -d '{
    "name": "winpe-mini-alpha",
    "total_size": 536870912,
    "block_count": 131072
  }'
```

---

## 11. 常见部署问题与解决

### 11.1 Rust编译问题

| 问题 | 症状 | 解决方案 |
|------|------|---------|
| RocksDB链接失败 | `undefined reference to rocksdb` | `sudo apt install librocksdb-dev` |
| protoc未找到 | `protoc not found` | `sudo apt install protobuf-compiler` |
| OpenSSL链接失败 | `openssl::Error` | `sudo apt install libssl-dev pkg-config` |
| cargo下载超时 | `failed to download from crates.io` | 设置镜像：`export CARGO_REGISTRY_MIRROR=https://mirrors.tuna.tsinghua.edu.cn/rust/` |
| 内存不足编译失败 | `signal: 9, SIGKILL` | 减少并行：`cargo build -j 2` |

### 11.2 服务启动问题

| 问题 | 症状 | 解决方案 |
|------|------|---------|
| PostgreSQL未就绪 | `connection refused` | 确保 `systemctl status postgresql` 为active |
| 端口被占用 | `Address already in use` | `ss -tlnp | grep <port>` 找到占用进程 |
| 权限不足 | `Permission denied` | 检查文件属主 `chown -R hyperdisk:hyperdisk /opt/hyperdisk` |
| io_uring不可用 | `io_uring_setup failed` | 检查内核版本 `uname -r`，需要≥6.1 |
| 配置文件找不到 | `No such file or directory` | 检查ExecStart中的--config路径 |

### 11.3 网络问题

| 问题 | 症状 | 解决方案 |
|------|------|---------|
| PXE无法启动 | 客户端卡在PXE | 检查dnsmasq配置+TFTP文件 |
| 镜像拉取超时 | `BlockRead timeout` | 检查存储网VLAN+ImageServer端口 |
| Nginx 502 | Bad Gateway | 检查后端Gateway服务是否运行 |
| DNS解析失败 | `name resolution failed` | 检查/etc/hosts或DNS配置 |

---

## 12. 部署检查清单

部署完成后，逐项确认：

### 基础环境
- [ ] Debian 13 内核 ≥ 6.1
- [ ] io_uring 可用（`ls /sys/kernel/debug/io_uring`）
- [ ] NVMe SSD 已识别（`lsblk`）
- [ ] rustc ≥ 1.80
- [ ] gcc ≥ 14.0
- [ ] cmake ≥ 3.24

### 编译
- [ ] metadata_center 编译成功
- [ ] api_gateway 编译成功
- [ ] image_server 编译成功（或已有二进制）
- [ ] 客户端exe/lib编译成功（Windows）

### 数据库
- [ ] PostgreSQL 运行中
- [ ] hyperdisk 数据库存在
- [ ] 8张表已创建
- [ ] 索引已创建

### 服务
- [ ] hyperdisk-metacenter 运行中（gRPC :50051）
- [ ] hyperdisk-gateway 运行中（HTTP :8080）
- [ ] hyperdisk-dna 运行中（gRPC :50052）
- [ ] hyperdisk-update 运行中（gRPC :50053）
- [ ] 所有服务开机自启

### 网络
- [ ] Nginx 运行中
- [ ] 反向代理工作正常（`curl /health` 返回 `{"status":"ok"}`）
- [ ] 管理控制台可访问
- [ ] PXE引导服务正常（dnsmasq）

### 监控
- [ ] Prometheus 运行中
- [ ] Grafana 运行中
- [ ] 指标采集正常

---

## 13. Alpha部署验证记录

> 以下为 2026-05-30 在 Debian 13 (192.168.2.80) 上的实际部署验证结果

### 环境信息

| 项目 | 值 |
|------|-----|
| OS | Debian 13 (Trixie) |
| 内核 | 6.12.73 |
| CPU | 6核 |
| RAM | 7.6GB |
| NVMe | 119GB SSD |
| Rust | rustc 1.96.0 + cargo 1.96.0 |
| GCC | 14.2 |
| CMake | 3.31 |
| PostgreSQL | 17.9 |
| RocksDB | 9.10 (librocksdb-dev) |
| liburing | 2.9 |

### 编译结果

| 服务 | 语言 | 编译时间 | 二进制大小 | 输出路径 |
|------|------|---------|-----------|---------|
| hd-metadata-center | Rust | ~3min (增量) | 15MB | target/release/hd-metadata-center |
| hd-api-gateway | Rust | ~3min (增量) | 4.5MB | target/release/hd-api-gateway |
| hd-dna-service | Rust | ~3min (增量) | 4.3MB | target/release/hd-dna-service |
| hd-update-service | Rust | ~3min (增量) | 4.4MB | target/release/hd-update-service |

> 首次完整编译约15-30分钟（含依赖下载+编译）

### 服务状态验证

```bash
# 所有服务运行状态
systemctl is-active hyperdisk-metacenter  # active
systemctl is-active hyperdisk-gateway     # active
systemctl is-active hyperdisk-dna         # active
systemctl is-active hyperdisk-update      # active

# 端口监听
ss -tlnp | grep -E "50051|50052|50053|8080|80"
# LISTEN 0.0.0.0:50051  (MetadataCenter gRPC)
# LISTEN 0.0.0.0:50052  (DNA Service gRPC)
# LISTEN 0.0.0.0:50053  (Update Service gRPC)
# LISTEN 0.0.0.0:8080   (API Gateway HTTP)
# LISTEN 0.0.0.0:80     (Nginx)

# 健康检查
curl http://192.168.2.80/health
# {"status":"ok","version":"0.1.0","uptime_seconds":5}
```

### 已知Alpha限制

| 限制 | 说明 | 计划 |
|------|------|------|
| ImageServer未部署 | C++20 ImageServer尚未在Debian编译 | Phase 2 |
| gRPC无认证 | MetadataCenter/DNA/Update服务无认证 | Phase 2 |
| 无PXE引导 | dnsmasq未配置 | Phase 2 |
| 无Prometheus | 监控未部署 | Phase 2 |
| Gateway无JWT | auth配置存在但未实现验证 | Phase 2 |
| 无SSL/HTTPS | Nginx仅HTTP | 生产前必须启用 |

---

*部署说明结束。下一步请参阅 [用户操作手册](usage.md) 和 [运维手册](operations.md)。*

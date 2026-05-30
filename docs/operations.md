# HTKIS HyperDisk X — 运维说明

> 版本：0.1.0 Alpha | 更新日期：2026-05-30

---

## 目录

1. [运维概述](#1-运维概述)
2. [服务管理](#2-服务管理)
3. [日志管理](#3-日志管理)
4. [监控与告警](#4-监控与告警)
5. [性能调优](#5-性能调优)
6. [故障排查](#6-故障排查)
7. [备份与恢复](#7-备份与恢复)
8. [扩容与缩容](#8-扩容与缩容)
9. [升级与回滚](#9-升级与回滚)
10. [安全运维](#10-安全运维)
11. [日常巡检](#11-日常巡检)
12. [应急响应](#12-应急响应)

---

## 1. 运维概述

### 1.1 运维原则

- **稳定优先**：宁可慢，不能坏。任何变更必须有回滚方案
- **变更窗口**：生产环境变更在凌晨2:00-6:00进行
- **灰度发布**：镜像更新必须通过灰度策略，5%→30%→100%
- **断网容忍**：服务端任何单点故障不应导致终端蓝屏

### 1.2 服务架构总览

```
                     ┌─────────────────┐
                     │   Nginx (80)    │
                     │   SSL终端        │
                     └────────┬────────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
    ┌─────────┴──────┐  ┌────┴─────┐  ┌──────┴──────────┐
    │ API Gateway    │  │Prometheus│  │   Grafana       │
    │ (8080) Rust    │  │ (9090)   │  │   (3000)        │
    └────────┬───────┘  └──────────┘  └─────────────────┘
             │ gRPC
    ┌────────┴────────┐
    │ MetadataCenter  │
    │ (50051) Rust    │
    │ RocksDB + PG    │
    └────────┬────────┘
             │
    ┌────────┴────────┐        ┌─────────────────┐
    │ ImageServer     │        │  PostgreSQL     │
    │ (9090) C++20    │        │  (5432)         │
    │ epoll+io_uring  │        │  配置+关系数据   │
    └─────────────────┘        └─────────────────┘
```

---

## 2. 服务管理

### 2.1 systemd服务操作

```bash
# 查看所有HyperDisk服务状态
systemctl status hyperdisk-metacenter hyperdisk-imageserver hyperdisk-gateway

# 启动/停止/重启单个服务
sudo systemctl start hyperdisk-metacenter
sudo systemctl stop hyperdisk-metacenter
sudo systemctl restart hyperdisk-metacenter

# 重载配置（不中断服务）
sudo systemctl reload hyperdisk-metacenter  # 如果服务支持reload

# 启用/禁用开机自启
sudo systemctl enable hyperdisk-metacenter
sudo systemctl disable hyperdisk-metacenter

# 查看服务是否开机自启
systemctl is-enabled hyperdisk-metacenter
```

### 2.2 服务依赖关系

```
PostgreSQL (5432)
    ↓
hyperdisk-metacenter (50051)
    ↓                    ↓
hyperdisk-gateway   hyperdisk-imageserver
   (8080)              (9090)
    ↓
Nginx (80/443)
```

**启动顺序**：PostgreSQL → MetadataCenter → ImageServer + Gateway → Nginx

**停止顺序**（反向）：Nginx → Gateway + ImageServer → MetadataCenter → PostgreSQL

### 2.3 快速启停脚本

```bash
#!/bin/bash
# /opt/hyperdisk/bin/hd-start.sh

echo "Starting HTKIS HyperDisk X..."
sudo systemctl start postgresql
sleep 2
sudo systemctl start hyperdisk-metacenter
sleep 3
sudo systemctl start hyperdisk-imageserver
sudo systemctl start hyperdisk-gateway
sleep 1
sudo systemctl reload nginx

echo "Verifying..."
for svc in hyperdisk-metacenter hyperdisk-imageserver hyperdisk-gateway; do
    if systemctl is-active --quiet $svc; then
        echo "  ✓ $svc"
    else
        echo "  ✗ $svc FAILED"
    fi
done
```

```bash
#!/bin/bash
# /opt/hyperdisk/bin/hd-stop.sh

echo "Stopping HTKIS HyperDisk X..."
sudo systemctl stop nginx
sudo systemctl stop hyperdisk-gateway
sudo systemctl stop hyperdisk-imageserver
sudo systemctl stop hyperdisk-metacenter
sudo systemctl stop postgresql

echo "All services stopped."
```

---

## 3. 日志管理

### 3.1 日志位置

| 服务 | 日志路径 | 格式 | 轮转 |
|------|---------|------|------|
| MetadataCenter | `/opt/hyperdisk/logs/metadata-center.log` | JSON | 每日，保留30天 |
| ImageServer | `/opt/hyperdisk/logs/image-server.log` | JSON | 每日，保留30天 |
| API Gateway | `/opt/hyperdisk/logs/api-gateway.log` | JSON | 每日，保留30天 |
| Nginx | `/var/log/nginx/access.log` | Combined | 每日，保留14天 |
| Nginx Error | `/var/log/nginx/error.log` | - | 每日，保留14天 |
| PostgreSQL | `/var/log/postgresql/` | CSV | 每日，保留7天 |
| systemd journal | `journalctl -u hyperdisk-*` | - | 系统管理 |

### 3.2 日志格式（JSON）

```json
{
  "timestamp": "2026-05-30T10:30:45.123Z",
  "level": "INFO",
  "service": "metadata-center",
  "module": "image_manager",
  "message": "Image registered",
  "image_id": 1,
  "name": "win11-gaming-v1",
  "request_id": "req-abc123"
}
```

### 3.3 日志查看

```bash
# 实时跟踪MetadataCenter日志
tail -f /opt/hyperdisk/logs/metadata-center.log | jq .

# 查看最近错误
grep '"level":"ERROR"' /opt/hyperdisk/logs/metadata-center.log | tail -20 | jq .

# 通过journalctl查看
journalctl -u hyperdisk-metacenter --since "1 hour ago" --no-pager

# 查看所有HyperDisk服务日志
journalctl -u "hyperdisk-*" --since "1 hour ago"
```

### 3.4 日志轮转配置

创建 `/etc/logrotate.d/hyperdisk`：

```
/opt/hyperdisk/logs/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    copytruncate
    dateext
    dateformat -%Y%m%d
}
```

启用：`sudo logrotate /etc/logrotate.d/hyperdisk`

---

## 4. 监控与告警

### 4.1 Prometheus配置

编辑 `/etc/prometheus/prometheus.yml`：

```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'hyperdisk-gateway'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: /metrics

  - job_name: 'hyperdisk-metacenter'
    static_configs:
      - targets: ['localhost:50051']
    # gRPC metrics通过gateway代理

  - job_name: 'node-exporter'
    static_configs:
      - targets: ['localhost:9100']

  - job_name: 'postgres-exporter'
    static_configs:
      - targets: ['localhost:9187']
```

### 4.2 关键监控指标

| 指标 | 采集源 | 告警阈值 | 说明 |
|------|--------|---------|------|
| `hd_block_read_latency_p95` | ImageServer | > 50ms | 块读延迟P95 |
| `hd_block_read_errors_total` | ImageServer | > 0 | 块读错误数 |
| `hd_cache_l1_hit_rate` | Client | < 0.80 | L1缓存命中率 |
| `hd_connected_terminals` | MetadataCenter | < expected | 在线终端数 |
| `hd_heartbeat_timeouts` | MetadataCenter | > 5/min | 心跳超时数 |
| `hd_reconnect_events` | Client | > 10/min | 断网重连事件 |
| `hd_image_serve_rate_mbps` | ImageServer | < 100 | 镜像服务吞吐 |
| `process_cpu_seconds_total` | All | > 0.8/core | CPU使用率 |
| `process_resident_memory_bytes` | All | > 80% limit | 内存使用 |

### 4.3 Grafana仪表盘

导入项目提供的Grafana仪表盘（JSON模型），包含以下面板：

1. **集群概览** — 在线终端数、镜像数、总吞吐
2. **块IO性能** — 延迟分布、吞吐、错误率
3. **缓存效率** — L1/L2/L3命中率趋势
4. **终端状态** — 各状态终端数量、断网事件
5. **服务健康** — CPU/内存/连接数/错误率

### 4.4 告警规则

编辑 `/etc/prometheus/alert_rules.yml`：

```yaml
groups:
  - name: hyperdisk_critical
    rules:
      - alert: ImageServerHighLatency
        expr: histogram_quantile(0.95, hd_block_read_latency_seconds_bucket) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "ImageServer块读延迟P95超过50ms"

      - alert: TerminalsMassDisconnect
        expr: rate(hd_heartbeat_timeouts[5m]) > 0.5
        for: 2m
        labels:
          severity: critical
        annotations:
          summary: "终端大规模断连（可能网络故障）"

      - alert: CacheHitRateLow
        expr: hd_cache_l1_hit_rate < 0.80
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "L1缓存命中率低于80%"

      - alert: ServiceDown
        expr: up{job=~"hyperdisk-.*"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "服务 {{ $labels.job }} 已宕机"
```

### 4.5 告警通知

配置AlertManager发送告警：

- **Critical**：短信+电话+企业微信
- **Warning**：企业微信+邮件
- **Info**：邮件

---

## 5. 性能调优

### 5.1 ImageServer调优

#### CPU亲和性绑定

ImageServer默认绑定CPU 0-7，确保IO线程不与其他服务争抢CPU：

```bash
# 查看当前亲和性
taskset -pc $(pgrep hd-image-server)

# 修改systemd服务的CPUAffinity
# 编辑 /etc/systemd/system/hyperdisk-imageserver.service
# CPUAffinity=0-7
```

#### io_uring参数

```toml
[server]
io_uring_entries = 256      # 提交队列深度（默认256，最大4096）
io_uring_flags = "SQPOLL"   # 内核侧轮询，减少系统调用
```

#### 网络缓冲区

```bash
# 增大TCP缓冲区
sudo sysctl -w net.core.rmem_max=16777216
sudo sysctl -w net.core.wmem_max=16777216
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# 持久化
cat >> /etc/sysctl.d/99-hyperdisk.conf << 'EOF'
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
EOF
sudo sysctl -p /etc/sysctl.d/99-hyperdisk.conf
```

### 5.2 MetadataCenter调优

#### RocksDB参数

```toml
[rocksdb]
cache_size_mb = 4096         # 块缓存大小（建议总内存的25-50%）
write_buffer_size_mb = 64    # MemTable大小
max_write_buffer_number = 4  # MemTable数量
level0_file_num_compaction_trigger = 4
target_file_size_base_mb = 64
max_bytes_for_level_base_mb = 256
```

#### gRPC连接池

```toml
[server]
grpc_max_concurrent_streams = 1000
grpc_keepalive_time_ms = 30000
grpc_keepalive_timeout_ms = 10000
```

### 5.3 PostgreSQL调优

详见 [部署说明 - PostgreSQL性能调优](deployment.md#83-postgresql性能调优)

关键参数回顾：
- `shared_buffers` = 总内存 × 25%
- `effective_cache_size` = 总内存 × 75%
- `work_mem` = 64MB（复杂查询）
- `random_page_cost` = 1.1（SSD）

### 5.4 客户端缓存调优

| 场景 | L1 RAM | L2 NVMe | 说明 |
|------|---------|---------|------|
| 网吧（8GB内存） | 2GB | 8GB | 默认配置 |
| 电竞酒店（16GB） | 4GB | 16GB | 更多RAM提升L1命中率 |
| 企业终端（8GB） | 1GB | 4GB | 保守配置，更多内存给应用 |
| 云桌面（4GB） | 512MB | - | 仅L1，无L2 |

---

## 6. 故障排查

### 6.1 终端0x7B蓝屏

**0x7B (INACCESSIBLE_BOOT_DEVICE)** 是最常见的无盘系统故障。

#### 排查步骤

```
1. 检查COM1串口输出
   └─ 连接USB串口线，115200 8N1，查看bootdiag输出

2. 检查bootdiag日志
   └─ WinPE中运行 bootdiag.exe --status
   └─ 查看哪项检查FAIL

3. 检查Boot Replay录制
   └─ 分析最近一次启动的 replay 文件
   └─ boot_replay_analyzer.exe --diff replay_good.bin replay_bad.bin

4. 检查网络连通性
   └─ 从终端ping服务器存储网IP
   └─ 确认VLAN 200配置正确

5. 检查服务端ImageServer
   └─ systemctl status hyperdisk-imageserver
   └─ 查看image-server.log是否有块读错误

6. 检查镜像完整性
   └─ 通过API验证镜像block_count和checksum
```

#### 常见原因

| 原因 | 症状 | 解决 |
|------|------|------|
| 镜像损坏 | 块校验和不匹配 | 重新上传镜像 |
| 驱动不匹配 | DNA分组错误 | 更新DNA分组+驱动层 |
| 网络故障 | 块读超时 | 检查VLAN+网线+交换机 |
| ImageServer宕机 | 连接拒绝 | 重启服务，检查日志 |
| BCD配置错误 | bootdiag BCD FAIL | 修复BCD，重新制作镜像 |
| 内存不足 | RamOverlay OOM | 增加终端内存或减小L1缓存 |

### 6.2 服务启动失败

```bash
# 查看详细错误
journalctl -u hyperdisk-metacenter -n 50 --no-pager

# 常见原因
# 1. PostgreSQL未就绪 → 启动顺序问题
# 2. 端口占用 → ss -tlnp | grep <port>
# 3. 配置文件错误 → 手动运行二进制检查报错
# 4. 权限不足 → 检查文件属主
# 5. RocksDB损坏 → 删除data/rocksdb重新初始化

# 手动运行（调试模式）
sudo -u hyperdisk /opt/hyperdisk/bin/hd-metadata-center --config /opt/hyperdisk/etc/metadata.toml
```

### 6.3 终端大规模断连

```
可能原因                排查方法
────────────────────────────────────────────
交换机故障               检查交换机状态灯+日志
网线/光纤断裂            物理检查
IP冲突                  arp -a 检查
DHCP耗尽                检查dnsmasq lease
ImageServer过载         检查CPU/内存/连接数
内核panic               dmesg | tail
```

### 6.4 镜像拉取慢

```bash
# 检查ImageServer吞吐
curl http://192.168.2.80/metrics | grep hd_image_serve_rate

# 检查网络带宽
iperf3 -c 10.10.200.10 -t 10 -P 4

# 检查磁盘IO
iostat -x 1 10 nvme0n1

# 检查io_uring是否生效
cat /proc/$(pgrep hd-image-server)/status | grep -i io
```

### 6.5 缓存命中率低

```bash
# 查看缓存统计
curl -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/terminals/{id}/cache-stats

# 如果L1命中率<80%：
#   - 增大L1缓存大小
#   - 检查是否有大量冷数据访问（新镜像？）
#   - 调整W-TinyLFU的Window/Protected比例

# 如果L2命中率低：
#   - 增大L2缓存大小
#   - 检查NVMe磁盘健康状态
#   - 考虑预热缓存（提前拉取热点块）
```

---

## 7. 备份与恢复

### 7.1 备份策略

| 数据 | 备份方式 | 频率 | 保留 | 位置 |
|------|---------|------|------|------|
| PostgreSQL | pg_dump | 每日 | 30天 | /opt/hyperdisk/backup/pg/ |
| RocksDB | 快照 | 每日 | 7天 | /opt/hyperdisk/backup/rocksdb/ |
| 镜像文件 | 增量同步 | 每周 | 4周 | 备份服务器 |
| 配置文件 | 全量拷贝 | 变更时 | 10版本 | /opt/hyperdisk/backup/config/ |
| Boot Replay | 归档 | 每周 | 90天 | /opt/hyperdisk/backup/replay/ |

### 7.2 自动备份脚本

```bash
#!/bin/bash
# /opt/hyperdisk/bin/hd-backup.sh

BACKUP_DIR="/opt/hyperdisk/backup"
DATE=$(date +%Y%m%d_%H%M%S)

# PostgreSQL备份
mkdir -p $BACKUP_DIR/pg
pg_dump -U hyperdisk -Fc hyperdisk > $BACKUP_DIR/pg/hyperdisk_$DATE.dump

# 保留最近30天
find $BACKUP_DIR/pg -name "*.dump" -mtime +30 -delete

# RocksDB备份（如果MetadataCenter支持）
# curl -X POST http://localhost:50051/admin/backup

# 配置文件备份
mkdir -p $BACKUP_DIR/config
tar czf $BACKUP_DIR/config/config_$DATE.tar.gz /opt/hyperdisk/etc/
find $BACKUP_DIR/config -name "*.tar.gz" -mtime +30 -delete

echo "Backup completed: $DATE"
```

### 7.3 PostgreSQL恢复

```bash
# 停止服务
sudo systemctl stop hyperdisk-gateway hyperdisk-imageserver hyperdisk-metacenter

# 恢复数据库
sudo -u postgres dropdb hyperdisk
sudo -u postgres createdb -O hyperdisk hyperdisk
pg_restore -U hyperdisk -d hyperdisk /opt/hyperdisk/backup/pg/hyperdisk_20260530.dump

# 重新初始化Schema（如果需要）
# PGPASSWORD=hyperdisk_2026 psql -U hyperdisk -d hyperdisk -f /opt/hyperdisk/config/schema.sql

# 重启服务
sudo systemctl start hyperdisk-metacenter
sleep 3
sudo systemctl start hyperdisk-imageserver hyperdisk-gateway
```

---

## 8. 扩容与缩容

### 8.1 水平扩容（增加ImageServer节点）

```bash
# 1. 在新服务器上部署ImageServer（参照部署说明）
# 2. 在MetadataCenter注册新节点

curl -X POST -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "hostname": "imageserver-02",
    "listen_addr": "10.10.200.11:9090",
    "role": "image_server",
    "capacity_bytes": 2199023255552
  }' \
  http://192.168.2.80/api/v1/server-nodes

# 3. MetadataCenter自动将终端负载均衡到新节点
# 4. 确认新节点健康
curl http://192.168.2.80/api/v1/server-nodes
```

### 8.2 垂直扩容（增加单节点资源）

```bash
# ImageServer内存扩容
# 1. 修改systemd的LimitMEMLOCK
#   编辑 /etc/systemd/system/hyperdisk-imageserver.service
#    LimitMEMLOCK=infinity

# 2. 增大RocksDB缓存
#    编辑 /opt/hyperdisk/etc/metadata.toml
#    [rocksdb]
#    cache_size_mb = 8192  # 原4096→8192

# 3. 重启服务
sudo systemctl daemon-reload
sudo systemctl restart hyperdisk-metacenter
```

### 8.3 缩容

```bash
# 标记节点为draining（停止接收新终端）
curl -X PUT -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"status": "draining"}' \
  http://192.168.2.80/api/v1/server-nodes/{node_id}

# 等待现有终端迁移到其他节点
# 确认节点无连接后停服
sudo systemctl stop hyperdisk-imageserver
```

---

## 9. 升级与回滚

### 9.1 服务端升级

```bash
# 1. 下载新版本二进制
scp hd-metadata-center.new debian@192.168.2.80:/opt/hyperdisk/bin/
scp hd-api-gateway.new debian@192.168.2.80:/opt/hyperdisk/bin/
scp hd-image-server.new debian@192.168.2.80:/opt/hyperdisk/bin/

# 2. 备份当前版本
cp /opt/hyperdisk/bin/hd-metadata-center /opt/hyperdisk/bin/hd-metadata-center.prev

# 3. 滚动升级（逐个服务）
# 先升级MetadataCenter
mv /opt/hyperdisk/bin/hd-metadata-center.new /opt/hyperdisk/bin/hd-metadata-center
sudo systemctl restart hyperdisk-metacenter
sleep 5
systemctl status hyperdisk-metacenter  # 确认正常

# 再升级ImageServer
mv /opt/hyperdisk/bin/hd-image-server.new /opt/hyperdisk/bin/hd-image-server
sudo systemctl restart hyperdisk-imageserver
sleep 5
systemctl status hyperdisk-imageserver

# 最后升级Gateway
mv /opt/hyperdisk/bin/hd-api-gateway.new /opt/hyperdisk/bin/hd-api-gateway
sudo systemctl restart hyperdisk-gateway
sleep 3
systemctl status hyperdisk-gateway
```

### 9.2 快速回滚

```bash
# 如果新版本有问题，立即回滚
cp /opt/hyperdisk/bin/hd-metadata-center.prev /opt/hyperdisk/bin/hd-metadata-center
sudo systemctl restart hyperdisk-metacenter

# 其他服务同理
```

### 9.3 镜像回滚

```bash
# 通过快照回滚
curl -X POST -H "Authorization: Bearer $TOKEN" \
  http://192.168.2.80/api/v1/snapshots/{snapshot_id}/rollback

# 或通过灰度策略自动回滚（故障率超阈值时）
```

---

## 10. 安全运维

### 10.1 JWT密钥轮换

```bash
# 1. 生成新密钥
NEW_SECRET=$(openssl rand -hex 32)

# 2. 更新gateway配置
# 编辑 /opt/hyperdisk/etc/gateway.toml
# [auth]
# jwt_secret = "$NEW_SECRET"

# 3. 重启Gateway
sudo systemctl restart hyperdisk-gateway

# 4. 所有用户需要重新登录
```

### 10.2 PostgreSQL密码轮换

```bash
# 修改数据库密码
sudo -u postgres psql -c "ALTER USER hyperdisk WITH PASSWORD 'new_password';"

# 更新MetadataCenter配置
# 编辑 /opt/hyperdisk/etc/metadata.toml [database] password

# 重启MetadataCenter
sudo systemctl restart hyperdisk-metacenter
```

### 10.3 防火墙配置

```bash
# 仅开放必要端口
sudo iptables -A INPUT -p tcp --dport 80 -j ACCEPT    # Nginx HTTP
sudo iptables -A INPUT -p tcp --dport 443 -j ACCEPT   # Nginx HTTPS
sudo iptables -A INPUT -p tcp --dport 9090 -j ACCEPT  # ImageServer (存储网)
sudo iptables -A INPUT -p udp --dport 69 -j ACCEPT    # TFTP (PXE网)
sudo iptables -A INPUT -p udp --dport 67 -j ACCEPT    # DHCP (PXE网)
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT    # SSH (管理网)

# 持久化
sudo apt install iptables-persistent
sudo netfilter-persistent save
```

### 10.4 审计日志

所有API请求通过Gateway记录审计日志：

```json
{
  "timestamp": "2026-05-30T10:30:45Z",
  "user": "admin",
  "action": "image.delete",
  "resource": "image_id=1",
  "source_ip": "10.10.300.100",
  "result": "success"
}
```

---

## 11. 日常巡检

### 11.1 每日巡检项

| 项目 | 命令/方法 | 正常标准 |
|------|---------|---------|
| 服务状态 | `systemctl status hyperdisk-*` | 全部active |
| 磁盘空间 | `df -h /opt/hyperdisk/data/` | 使用率<80% |
| 内存使用 | `free -h` | 可用>20% |
| 在线终端 | API `/api/v1/terminals?status=online` | 数量符合预期 |
| 心跳超时 | Prometheus `hd_heartbeat_timeouts` | <5/min |
| 块读错误 | Prometheus `hd_block_read_errors_total` | =0 |
| 日志错误 | `grep ERROR logs/*.log` | 无CRITICAL |

### 11.2 每周巡检项

| 项目 | 命令/方法 | 说明 |
|------|---------|------|
| 缓存命中率趋势 | Grafana面板 | 持续<85%需调优 |
| RocksDB统计 | `curl localhost:50051/admin/rocksdb-stats` | compactionpending=0 |
| PostgreSQL慢查询 | `pg_stat_statements` | >1s的查询需优化 |
| 备份验证 | 恢复到测试环境 | 确保备份可用 |
| NVMe健康 | `smartctl -a /dev/nvme0n1` | available_spare>10% |

### 11.3 自动巡检脚本

```bash
#!/bin/bash
# /opt/hyperdisk/bin/hd-healthcheck.sh

echo "=== HTKIS HyperDisk X Health Check ==="
echo "Date: $(date)"
echo ""

# 1. 服务状态
echo "--- Service Status ---"
for svc in hyperdisk-metacenter hyperdisk-imageserver hyperdisk-gateway; do
    status=$(systemctl is-active $svc 2>/dev/null)
    if [ "$status" = "active" ]; then
        echo "  [OK] $svc"
    else
        echo "  [FAIL] $svc ($status)"
    fi
done

# 2. 磁盘空间
echo ""
echo "--- Disk Usage ---"
df -h /opt/hyperdisk/data/ | tail -1

# 3. 内存
echo ""
echo "--- Memory ---"
free -h | head -2

# 4. 端口监听
echo ""
echo "--- Ports ---"
ss -tlnp | grep -E '50051|8080|9090|80|443|5432'

# 5. 最近错误
echo ""
echo "--- Recent Errors (last 1h) ---"
find /opt/hyperdisk/logs/ -name "*.log" -mmin -60 \
    -exec grep -l '"level":"ERROR"' {} \; 2>/dev/null | head -5

echo ""
echo "=== Health Check Complete ==="
```

---

## 12. 应急响应

### 12.1 ImageServer宕机

```
影响: 所有终端无法读取新块（L1/L2缓存可继续服务已缓存数据）
响应时间: <5分钟

步骤:
1. 确认宕机: systemctl status hyperdisk-imageserver
2. 查看日志: journalctl -u hyperdisk-imageserver -n 100
3. 尝试重启: sudo systemctl restart hyperdisk-imageserver
4. 如果重启失败:
   a. 检查NVMe磁盘: smartctl -a /dev/nvme0n1
   b. 检查内存: dmesg | grep -i oom
   c. 切换到备用ImageServer（如果有）
5. 通知终端: 断网终端自动进入RamOverlay模式
```

### 12.2 MetadataCenter宕机

```
影响: 无法注册新终端/查询镜像信息，但现有连接不受影响
响应时间: <5分钟

步骤:
1. 确认宕机: systemctl status hyperdisk-metacenter
2. 检查PostgreSQL: systemctl status postgresql
3. 如果PG正常，尝试重启MC
4. 如果PG异常，先修复PG再启动MC
```

### 12.3 存储网整体故障

```
影响: 所有终端断网，进入RamOverlay模式
响应时间: <10分钟

步骤:
1. 确认故障范围: 从服务器ping交换机+终端
2. 检查交换机: 端口状态、VLAN配置
3. 检查网卡: ethtool <iface>
4. 终端自动进入RamOverlay模式，不会蓝屏
5. 修复网络后终端自动恢复（5秒检测+指数退避重连）
6. 恢复后脏块自动同步
```

### 12.4 大规模0x7B

```
影响: 多台终端同时蓝屏
响应时间: 立即

步骤:
1. 立即停止所有终端PXE启动（关闭DHCP/TFTP）
2. 检查最近是否有镜像更新
3. 如果有更新 → 回滚到上一版本镜像
4. 检查ImageServer是否正常服务
5. 检查网络连通性
6. 排查单台终端COM1输出确认根因
7. 修复后灰度恢复（5%→30%→100%）
```

---

## 13. Alpha部署运维参考

### 13.1 服务完整列表

| 服务 | systemd名称 | 类型 | 端口 | 二进制路径 |
|------|------------|------|------|-----------|
| MetadataCenter | `hyperdisk-metacenter` | gRPC | 50051 | `/opt/hyperdisk/bin/hd-metadata-center` |
| API Gateway | `hyperdisk-gateway` | HTTP | 8080 | `/opt/hyperdisk/bin/hd-api-gateway` |
| DNA Service | `hyperdisk-dna` | gRPC | 50052 | `/opt/hyperdisk/bin/hd-dna-service` |
| Update Service | `hyperdisk-update` | gRPC | 50053 | `/opt/hyperdisk/bin/hd-update-service` |
| Nginx | `nginx` | HTTP | 80/443 | 系统包 |
| PostgreSQL | `postgresql` | TCP | 5432 | 系统包 |

### 13.2 配置文件位置

| 文件 | 路径 | 说明 |
|------|------|------|
| MetadataCenter配置 | `/opt/hyperdisk/etc/metadata.toml` | gRPC监听+RocksDB+日志 |
| API Gateway配置 | `/opt/hyperdisk/etc/gateway.toml` | HTTP监听+gRPC地址+日志 |
| Nginx配置 | `/etc/nginx/sites-available/hyperdisk` | 反向代理+静态资源 |
| PostgreSQL配置 | `/etc/postgresql/17/main/postgresql.conf` | 连接+内存+WAL |
| PostgreSQL Schema | `/opt/hyperdisk/config/schema.sql` | 8表+索引定义 |
| systemd服务 | `/etc/systemd/system/hyperdisk-*.service` | 服务单元文件 |

### 13.3 实际日志位置

| 服务 | 日志路径 | 说明 |
|------|---------|------|
| MetadataCenter | `journalctl -u hyperdisk-metacenter` | JSON格式，stdout |
| API Gateway | `journalctl -u hyperdisk-gateway` | JSON格式，stdout |
| DNA Service | `journalctl -u hyperdisk-dna` | JSON格式，stdout |
| Update Service | `journalctl -u hyperdisk-update` | JSON格式，stdout |
| Nginx | `/var/log/nginx/access.log` | Combined格式 |
| Nginx Error | `/var/log/nginx/error.log` | 错误日志 |
| PostgreSQL | `/var/log/postgresql/` | CSV格式 |

### 13.4 快速诊断命令

```bash
# 一键检查所有服务状态
for svc in hyperdisk-metacenter hyperdisk-gateway hyperdisk-dna hyperdisk-update; do
    echo "$svc: $(systemctl is-active $svc)"
done

# 一键检查端口
ss -tlnp | grep -E "50051|50052|50053|8080|80|5432"

# 一键健康检查
curl -s http://localhost:8080/health | python3 -m json.tool

# 查看所有服务最近日志
journalctl -u "hyperdisk-*" --since "10 minutes ago" --no-pager

# 检查磁盘空间
df -h /opt/hyperdisk/

# 检查RocksDB数据大小
du -sh /opt/hyperdisk/data/rocksdb/
```

### 13.5 服务重启顺序

```bash
# 正确的启动顺序
sudo systemctl start postgresql
sudo systemctl start hyperdisk-metacenter    # 依赖PG
sleep 3
sudo systemctl start hyperdisk-gateway       # 依赖MC
sudo systemctl start hyperdisk-dna
sudo systemctl start hyperdisk-update
sudo systemctl reload nginx

# 正确的停止顺序（反向）
sudo systemctl reload nginx                  # 先停止接收新请求
sudo systemctl stop hyperdisk-gateway
sudo systemctl stop hyperdisk-dna
sudo systemctl stop hyperdisk-update
sudo systemctl stop hyperdisk-metacenter
sudo systemctl stop postgresql               # 最后停PG
```

### 13.6 常见运维操作速查

```bash
# 查看MetadataCenter gRPC服务列表
grpcurl -plaintext localhost:50051 list

# 查看Gateway版本和运行时间
curl -s http://localhost:8080/health | python3 -m json.tool

# 查看Prometheus指标
curl -s http://localhost:8080/metrics

# 重载Nginx配置（不中断服务）
sudo systemctl reload nginx

# 重载PostgreSQL配置（不中断服务）
sudo -u postgres psql -c "SELECT pg_reload_conf();"

# 查看PostgreSQL活跃连接
sudo -u postgres psql -c "SELECT count(*) FROM pg_stat_activity;"

# 查看PostgreSQL数据库大小
sudo -u postgres psql -c "SELECT pg_size_pretty(pg_database_size('hyperdisk'));"
```

---

*运维手册结束。返回 [部署教程](deployment.md) 或 [用户操作手册](usage.md)。*

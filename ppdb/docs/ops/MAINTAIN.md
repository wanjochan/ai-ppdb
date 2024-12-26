# PPDB 运维手册

## 1. 日常运维

### 1.1 服务管理
```bash
# 启动服务
systemctl start ppdb

# 停止服务
systemctl stop ppdb

# 重启服务
systemctl restart ppdb

# 查看服务状态
systemctl status ppdb
```

### 1.2 日志管理
```bash
# 查看实时日志
tail -f /var/log/ppdb/ppdb.log

# 日志轮转
logrotate /etc/logrotate.d/ppdb

# 日志分析
grep "ERROR" /var/log/ppdb/ppdb.log
```

## 2. 监控运维

### 2.1 系统监控
```bash
# 查看系统资源
top
htop
iostat
vmstat

# 查看网络连接
netstat -anp | grep ppdb
```

### 2.2 性能监控
```bash
# 查看性能指标
curl http://localhost:7000/metrics

# 性能分析
perf record -p $(pgrep ppdb)
perf report
```

## 3. 备份恢复

### 3.1 数据备份
```bash
# 创建快照
ppdb-cli snapshot create

# 备份数据
tar -czf ppdb-backup-$(date +%Y%m%d).tar.gz /var/lib/ppdb/data

# 备份配置
cp -r /etc/ppdb /etc/ppdb.bak
```

### 3.2 数据恢复
```bash
# 恢复数据
tar -xzf ppdb-backup-20231225.tar.gz -C /var/lib/ppdb/

# 恢复配置
cp -r /etc/ppdb.bak/* /etc/ppdb/
```

## 4. 集群运维

### 4.1 节点管理
```bash
# 查看节点状态
ppdb-cli cluster status

# 添加节点
ppdb-cli cluster add-node --host node2 --port 7000

# 移除节点
ppdb-cli cluster remove-node node2:7000
```

### 4.2 数据迁移
```bash
# 启动数据迁移
ppdb-cli migrate start --source node1 --target node2

# 查看迁移进度
ppdb-cli migrate status
```

## 5. 故障处理

### 5.1 常见问题
1. 服务无法启动
```bash
# 检查错误日志
journalctl -u ppdb -n 100

# 检查端口占用
netstat -tulpn | grep 7000

# 检查文件权限
ls -l /var/lib/ppdb/
```

2. 性能下降
```bash
# 检查系统负载
uptime

# 检查IO状态
iostat -x 1

# 检查网络状态
iftop
```

### 5.2 故障恢复
1. 数据损坏
```bash
# 检查数据文件
ppdb-cli check /var/lib/ppdb/data

# 从备份恢复
ppdb-cli restore backup-20231225
```

2. 节点故障
```bash
# 隔离故障节点
ppdb-cli cluster isolate node2:7000

# 恢复节点
ppdb-cli cluster recover node2:7000
```

## 6. 升级维护

### 6.1 版本升级
```bash
# 1. 备份数据
ppdb-cli backup create

# 2. 停止服务
systemctl stop ppdb

# 3. 更新软件包
dpkg -i ppdb_1.1.0_amd64.deb

# 4. 启动服务
systemctl start ppdb

# 5. 验证升级
ppdb-cli version
```

### 6.2 配置更新
```bash
# 1. 备份配置
cp /etc/ppdb/ppdb.conf /etc/ppdb/ppdb.conf.bak

# 2. 修改配置
vim /etc/ppdb/ppdb.conf

# 3. 验证配置
ppdb-cli check-config /etc/ppdb/ppdb.conf

# 4. 重载配置
ppdb-cli reload
```

## 7. 性能优化

### 7.1 系统优化
```bash
# 调整系统参数
sysctl -w vm.swappiness=10
sysctl -w vm.dirty_ratio=40
sysctl -w vm.dirty_background_ratio=10

# 调整文件描述符
ulimit -n 65535
```

### 7.2 数据库优化
```yaml
# ppdb.conf

# 内存配置
memory:
  cache_size: 4GB
  write_buffer: 64MB

# 存储配置
storage:
  compression: true
  block_size: 4KB
  
# 性能配置
performance:
  sync_write: false
  batch_size: 1000
```

## 8. 安全维护

### 8.1 访问控制
```bash
# 更新访问令牌
ppdb-cli token rotate

# 查看访问日志
tail -f /var/log/ppdb/access.log
```

### 8.2 安全审计
```bash
# 查看审计日志
ausearch -p $(pgrep ppdb)

# 检查文件完整性
tripwire --check
```

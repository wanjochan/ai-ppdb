# PPDB 客户端指南

PPDB 提供了两种客户端交互方式：服务端内置客户端和独立客户端工具。

## 1. 服务端内置客户端

服务端启动时自动包含一个本地CLI，无需认证即可访问：
```bash
$ ppdb_server
Starting PPDB server on port 11211...
[Local CLI] Auto-connected with admin privileges
ppdb> auth status
Current user: admin (local)
Privileges: all

ppdb> help
Available commands:
  auth status            显示当前认证状态
  auth list             列出所有用户
  auth create <user>    创建新用户
  auth grant <priv>     授予权限
  get <key>            获取键值
  set <key> <value>    设置键值
  delete <key>         删除键值
  stats                显示服务器状态
  clients              显示已连接客户端
  config get <param>   获取配置
  config set <param>   修改配置
  help                 显示帮助
  exit                 退出（服务器继续运行）
```

### 特点
- 直接访问本地数据库，无网络开销
- 实时监控服务器状态
- 支持配置热更新
- 命令行自动补全
- 历史记录支持

## 2. 远程客户端工具

独立的命令行客户端，需要认证才能连接服务器：
```bash
$ ppdb_cli --help
Usage: ppdb_cli [options] [command]

Options:
  -h, --host <host>     服务器地址 (默认: localhost)
  -p, --port <port>     服务器端口 (默认: 11211)
  -u, --user <user>     用户名
  -P, --password        提示输入密码
  --auth-file <file>    认证配置文件
  -t, --timeout <ms>    超时时间 (默认: 1000ms)
  --help                显示帮助

# 交互式登录
$ ppdb_cli
ppdb> connect localhost 11211
Username: admin
Password: ****
Connected to ppdb server.
ppdb> auth status
Current user: admin
Privileges: read, write, admin

# 命令行登录
$ ppdb_cli -u admin -P
Password: ****
ppdb> 

# 使用认证文件
$ cat ~/.ppdb/auth.conf
host=localhost
port=11211
user=admin
password=****

$ ppdb_cli --auth-file ~/.ppdb/auth.conf
Connected to ppdb server.
ppdb>
```

### 特点
- 支持交互式和命令行两种模式
- 批处理支持
- 格式化输出
- 连接池优化
- 错误重试机制

## 3. 认证与权限管理

### 权限级别
- **admin**: 完全访问权限，包括用户管理
- **write**: 读写数据权限
- **read**: 只读权限
- **stats**: 只能查看统计信息

### 用户管理命令
```bash
# 创建新用户
ppdb> auth create john
New password: ****
Confirm password: ****
User 'john' created.

# 授予权限
ppdb> auth grant john write
Granted 'write' privilege to user 'john'.

# 查看用户列表
ppdb> auth list
Users:
  admin: all
  john: read, write
  guest: read
```

### 安全建议
1. 定期更改密码
2. 使用强密码
3. 根据最小权限原则分配权限
4. 避免在脚本中明文存储密码

## 4. 编程接口中的认证

### C 客户端库
```c
ppdb_client_t* client = ppdb_client_create();
ppdb_client_set_credentials(client, "admin", "password");
ppdb_client_connect(client, "localhost", 11211);
```

### Python 客户端库
```python
from ppdb import Client

client = Client('localhost', 11211, 
                username='admin',
                password='password')

# 或者从配置文件
client = Client.from_config('~/.ppdb/auth.conf')
```

## 5. 最佳实践

### 认证安全
- 使用环境变量存储敏感信息
- 实现密码轮换机制
- 启用SSL/TLS加密
- 监控认证失败事件

### 权限管理
- 创建专用服务账号
- 定期审计用户权限
- 删除未使用的账号
- 记录权限变更日志

## 6. 编程接口

### C 客户端库
```c
#include <ppdb/client.h>

// 创建客户端
ppdb_client_t* client = ppdb_client_create();
ppdb_client_connect(client, "localhost", 11211);

// 异步操作
void on_response(ppdb_client_t* client, const char* value) {
    printf("Got value: %s\n", value);
}

ppdb_client_get_async(client, "key", on_response);

// 同步操作
char* value = ppdb_client_get(client, "key");
ppdb_client_set(client, "key", "value");

// 批量操作
ppdb_batch_t* batch = ppdb_batch_create();
ppdb_batch_set(batch, "k1", "v1");
ppdb_batch_set(batch, "k2", "v2");
ppdb_client_execute_batch(client, batch);

// 清理
ppdb_batch_free(batch);
ppdb_client_free(client);
```

### Python 客户端库
```python
from ppdb import Client

# 同步客户端
client = Client('localhost', 11211)
client.set('key', 'value')
value = client.get('key')

# 异步客户端
async with AsyncClient('localhost', 11211) as client:
    await client.set('key', 'value')
    value = await client.get('key')
    
# 批量操作
with client.batch() as batch:
    batch.set('k1', 'v1')
    batch.set('k2', 'v2')
```

## 7. 运维建议

### 连接管理
- 使用连接池
- 及时关闭不用的连接
- 设置合适的超时时间

### 错误处理
- 实现重试机制
- 优雅降级
- 错误日志记录

### 性能优化
- 使用批量操作
- 启用 pipeline
- 合理设置缓冲区大小

### 运维建议
- 使用内置客户端进行监控
- 编写管理脚本
- 定期检查服务器状态
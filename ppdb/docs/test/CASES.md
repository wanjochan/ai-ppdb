# PPDB 测试用例

## 1. 单元测试

### 1.1 存储引擎测试
```c
TEST_CASE("MemTable基本操作") {
    SECTION("插入和查询") {
        MemTable* table = memtable_create();
        
        // 测试插入
        REQUIRE(memtable_put(table, "key1", "value1") == 0);
        
        // 测试查询
        char* value = NULL;
        REQUIRE(memtable_get(table, "key1", &value) == 0);
        REQUIRE(strcmp(value, "value1") == 0);
        
        memtable_destroy(table);
    }
    
    SECTION("删除") {
        // 删除测试用例
    }
}

TEST_CASE("WAL操作") {
    SECTION("日志写入") {
        // WAL写入测试
    }
    
    SECTION("日志恢复") {
        // WAL恢复测试
    }
}
```

### 1.2 网络组件测试
```c
TEST_CASE("网络连接") {
    SECTION("建立连接") {
        // 连接建立测试
    }
    
    SECTION("数据传输") {
        // 数据传输测试
    }
}
```

### 1.3 一致性协议测试
```c
TEST_CASE("Raft协议") {
    SECTION("选举") {
        // 领导者选举测试
    }
    
    SECTION("日志复制") {
        // 日志复制测试
    }
}
```

## 2. 集成测试

### 2.1 API接口测试
```c
TEST_CASE("HTTP API") {
    SECTION("PUT请求") {
        // 发送PUT请求
        Response resp = http_put("/kv/test", "value");
        REQUIRE(resp.status_code == 200);
    }
    
    SECTION("GET请求") {
        // 发送GET请求
        Response resp = http_get("/kv/test");
        REQUIRE(resp.status_code == 200);
        REQUIRE(resp.body == "value");
    }
}
```

### 2.2 集群操作测试
```c
TEST_CASE("集群操作") {
    SECTION("节点加入") {
        // 测试节点加入集群
    }
    
    SECTION("数据复制") {
        // 测试数据复制
    }
}
```

## 3. 性能测试

### 3.1 基准测试
```c
BENCHMARK("写入性能") {
    // 准备数据
    const int count = 1000000;
    char key[16], value[100];
    
    // 批量写入
    for (int i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        db_put(key, value);
    }
}

BENCHMARK("读取性能") {
    // 随机读取测试
}
```

### 3.2 压力测试
```c
TEST_CASE("并发写入", "[stress]") {
    const int thread_count = 8;
    const int ops_per_thread = 100000;
    
    // 创建线程池
    ThreadPool pool(thread_count);
    
    // 提交任务
    for (int i = 0; i < thread_count; i++) {
        pool.submit([=]() {
            for (int j = 0; j < ops_per_thread; j++) {
                // 执行写入操作
            }
        });
    }
}
```

## 4. 故障测试

### 4.1 节点故障
```c
TEST_CASE("节点故障恢复") {
    SECTION("领导者故障") {
        // 1. 设置3节点集群
        // 2. 关闭领导者节点
        // 3. 验证新领导者选举
        // 4. 验证系统可用性
    }
    
    SECTION("跟随者故障") {
        // 跟随者节点故障测试
    }
}
```

### 4.2 网络故障
```c
TEST_CASE("网络分区") {
    SECTION("脑裂场景") {
        // 1. 创建网络分区
        // 2. 验证系统行为
        // 3. 恢复网络连接
        // 4. 验证数据一致性
    }
}
```

## 5. 一致性测试

### 5.1 线性一致性
```c
TEST_CASE("线性一致性") {
    SECTION("并发写入") {
        // 1. 多客户端并发写入
        // 2. 验证写入顺序
        // 3. 验证读取结果
    }
}
```

### 5.2 故障一致性
```c
TEST_CASE("故障一致性") {
    SECTION("故障期间的一致性") {
        // 1. 注入故障
        // 2. 执行操作
        // 3. 验证一致性
    }
}
```

## 6. 长期稳定性测试

### 6.1 持久化测试
```c
TEST_CASE("长期运行", "[long]") {
    SECTION("持续写入") {
        // 持续24小时写入测试
    }
    
    SECTION("定期重启") {
        // 定期重启节点测试
    }
}
```

### 6.2 资源泄露测试
```c
TEST_CASE("资源泄露", "[long]") {
    SECTION("内存泄露") {
        // 使用valgrind检测内存泄露
    }
    
    SECTION("文件句柄") {
        // 检测文件句柄泄露
    }
}
```

## 7. 最新测试结果 (2024-12-26)

### 7.1 KVStore测试套件
- ✅ create_close：通过
  - 测试KVStore的创建和关闭功能
  - 验证了WAL目录的创建和清理
  - 所有操作正常完成

- ✅ null_params：通过
  - 测试参数验证功能
  - 验证了对空参数的处理
  - 符合预期行为

- ✅ basic_ops：通过
  - 测试基本的Put/Get/Delete操作
  - WAL记录正确写入
  - 基本操作功能正常

- ❌ recovery：失败
  - 错误：Get recovered data (error: -5)
  - 问题：从MemTable中读取恢复的数据失败
  - 需要调查：WAL恢复过程中数据重放的问题

- ✅ concurrent：通过
  - 测试并发操作
  - 4个线程并发读写，每个线程100次操作
  - 所有操作成功完成，无数据竞争
  - 总计完成800次成功操作

### 7.2 MemTable测试套件
- ✅ create：通过
  - 测试MemTable的创建和销毁
  - 内存管理正常

- ✅ basic_ops：通过
  - 测试基本的Put/Get操作
  - 数据正确存储和检索

- ✅ delete：通过
  - 测试删除操作
  - 验证删除后的状态

- ✅ size_limit：通过
  - 测试大小限制功能
  - 当超过32字节限制时正确拒绝

### 7.3 WAL测试套件
- ✅ basic：通过
  - 测试基本的WAL操作
  - 包括写入和删除记录
  - WAL文件正确创建和管理

- ✅ concurrent：通过
  - 测试并发WAL操作
  - 验证了并发写入的正确性
  - WAL段文件管理正常

### 7.4 后续行动项
1. 调查并修复KVStore recovery测试失败的问题
   - 检查WAL记录的重放过程
   - 验证MemTable的数据恢复逻辑
   - 添加更详细的错误日志

2. 改进测试覆盖率
   - 添加更多边界条件测试
   - 增加错误注入测试
   - 完善并发测试场景

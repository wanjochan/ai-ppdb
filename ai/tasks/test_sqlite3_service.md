# SQLite3 服务测试计划

## 问题分析

SQLite3 服务已经在 5433 端口启动，需要设计测试方案。服务特点：
1. 支持内存数据库（`:memory:`）和文件数据库
2. 使用标准 SQLite3 协议
3. 支持基本的 SQL 操作

## 测试方案

### 1. 基础功能测试
- 连接测试
- 基本 SQL 操作（CREATE/INSERT/SELECT/UPDATE/DELETE）
- 事务支持
- 并发连接测试

### 2. 测试工具选择

两种方案：
1. 使用 Python 的 sqlite3 模块（推荐）
   - 标准库自带
   - 接口简单
   - 支持异步操作
   - 方便进行自动化测试

2. 命令行工具 sqlite3
   - 适合手动测试和调试
   - 可以快速验证服务状态

### 3. 测试用例设计

```python
import sqlite3
import unittest
import threading
import time

class TestSQLite3Service(unittest.TestCase):
    def setUp(self):
        self.conn = sqlite3.connect('localhost:5433')
        self.cur = self.conn.cursor()
    
    def tearDown(self):
        self.cur.close()
        self.conn.close()
    
    def test_basic_operations(self):
        # 创建表
        self.cur.execute('''
            CREATE TABLE test (
                id INTEGER PRIMARY KEY,
                name TEXT,
                value REAL
            )
        ''')
        
        # 插入数据
        self.cur.execute('INSERT INTO test VALUES (1, "test1", 1.1)')
        self.cur.execute('INSERT INTO test VALUES (2, "test2", 2.2)')
        self.conn.commit()
        
        # 查询数据
        self.cur.execute('SELECT * FROM test')
        rows = self.cur.fetchall()
        self.assertEqual(len(rows), 2)
        
        # 更新数据
        self.cur.execute('UPDATE test SET value = 3.3 WHERE id = 1')
        self.conn.commit()
        
        # 验证更新
        self.cur.execute('SELECT value FROM test WHERE id = 1')
        value = self.cur.fetchone()[0]
        self.assertEqual(value, 3.3)
        
        # 删除数据
        self.cur.execute('DELETE FROM test WHERE id = 2')
        self.conn.commit()
        
        # 验证删除
        self.cur.execute('SELECT COUNT(*) FROM test')
        count = self.cur.fetchone()[0]
        self.assertEqual(count, 1)

    def test_transaction(self):
        # 创建表
        self.cur.execute('CREATE TABLE trans_test (id INTEGER PRIMARY KEY, value TEXT)')
        
        try:
            # 开始事务
            self.cur.execute('BEGIN TRANSACTION')
            self.cur.execute('INSERT INTO trans_test VALUES (1, "value1")')
            self.cur.execute('INSERT INTO trans_test VALUES (2, "value2")')
            # 故意制造错误
            self.cur.execute('INSERT INTO trans_test VALUES (2, "value3")')  # 主键冲突
            self.conn.commit()
        except sqlite3.Error:
            self.conn.rollback()
        
        # 验证回滚
        self.cur.execute('SELECT COUNT(*) FROM trans_test')
        count = self.cur.fetchone()[0]
        self.assertEqual(count, 0)

    def test_concurrent_connections(self):
        def worker(worker_id):
            conn = sqlite3.connect('localhost:5433')
            cur = conn.cursor()
            try:
                cur.execute('CREATE TABLE IF NOT EXISTS concurrent_test (id INTEGER PRIMARY KEY, worker_id INTEGER)')
                cur.execute('INSERT INTO concurrent_test VALUES (?, ?)', (worker_id, worker_id))
                conn.commit()
            finally:
                cur.close()
                conn.close()
        
        # 创建多个线程并发访问
        threads = []
        for i in range(10):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()
        
        # 等待所有线程完成
        for t in threads:
            t.join()
        
        # 验证结果
        self.cur.execute('SELECT COUNT(*) FROM concurrent_test')
        count = self.cur.fetchone()[0]
        self.assertEqual(count, 10)

if __name__ == '__main__':
    unittest.main()
```

## 执行计划

1. 启动服务
```bash
./ppdb/ppdb_latest.exe sqlite3 --start --db=:memory:
```

2. 运行测试
```bash
python3 test_sqlite3_service.py
```

3. 手动验证（可选）
```bash
sqlite3 localhost:5433
```

## 预期结果

1. 所有测试用例应该通过
2. 服务应该能稳定处理并发连接
3. 事务应该正确回滚
4. 内存数据库应该在服务重启后清空 
#!/usr/bin/env python3

"""
PPDB 基本使用场景测试
验证系统在真实使用场景下的表现
"""

import unittest
import requests
import json
import time
import subprocess
import os
import signal

class PPDBBasicScenarioTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """准备测试环境"""
        cls.ppdb_port = 7000
        cls.base_url = f"http://localhost:{cls.ppdb_port}"
        cls.data_dir = "/tmp/ppdb_test"
        
        # 清理并创建测试目录
        os.system(f"rm -rf {cls.data_dir}")
        os.makedirs(cls.data_dir, exist_ok=True)
        
        # 启动服务器
        cls.server_process = subprocess.Popen(
            ["../../build/ppdb", "--port", str(cls.ppdb_port), 
             "--data-dir", cls.data_dir]
        )
        time.sleep(2)  # 等待服务器启动
    
    @classmethod
    def tearDownClass(cls):
        """清理测试环境"""
        cls.server_process.send_signal(signal.SIGTERM)
        cls.server_process.wait()
        os.system(f"rm -rf {cls.data_dir}")
    
    def test_user_management(self):
        """测试用户管理场景"""
        # 创建用户
        user_data = {
            "name": "Alice",
            "email": "alice@example.com",
            "age": 25
        }
        response = requests.put(
            f"{self.base_url}/kv/user:1",
            json={"value": json.dumps(user_data)}
        )
        self.assertEqual(response.status_code, 200)
        
        # 读取用户信息
        response = requests.get(f"{self.base_url}/kv/user:1")
        self.assertEqual(response.status_code, 200)
        stored_user = json.loads(response.json()["value"])
        self.assertEqual(stored_user["name"], "Alice")
        
        # 更新用户信息
        user_data["age"] = 26
        response = requests.put(
            f"{self.base_url}/kv/user:1",
            json={"value": json.dumps(user_data)}
        )
        self.assertEqual(response.status_code, 200)
        
        # 验证更新
        response = requests.get(f"{self.base_url}/kv/user:1")
        stored_user = json.loads(response.json()["value"])
        self.assertEqual(stored_user["age"], 26)
    
    def test_batch_operations(self):
        """测试批量操作场景"""
        # 批量写入
        for i in range(10):
            data = {
                "item_id": f"ITEM_{i}",
                "price": i * 10.5,
                "stock": i * 5
            }
            response = requests.put(
                f"{self.base_url}/kv/item:{i}",
                json={"value": json.dumps(data)}
            )
            self.assertEqual(response.status_code, 200)
        
        # 批量读取和验证
        for i in range(10):
            response = requests.get(f"{self.base_url}/kv/item:{i}")
            self.assertEqual(response.status_code, 200)
            item = json.loads(response.json()["value"])
            self.assertEqual(item["item_id"], f"ITEM_{i}")
            self.assertEqual(item["price"], i * 10.5)
    
    def test_error_handling(self):
        """测试错误处理场景"""
        # 测试访问不存在的键
        response = requests.get(f"{self.base_url}/kv/nonexistent")
        self.assertEqual(response.status_code, 404)
        
        # 测试无效的 JSON 数据
        response = requests.put(
            f"{self.base_url}/kv/test",
            data="invalid json"
        )
        self.assertNotEqual(response.status_code, 200)
        
        # 测试无效的 URL
        response = requests.get(f"{self.base_url}/invalid/path")
        self.assertEqual(response.status_code, 404)

if __name__ == '__main__':
    unittest.main()

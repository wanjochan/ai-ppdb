#!/usr/bin/env python3

"""
PPDB HTTP API CRUD 测试
测试 HTTP API 的基本 CRUD 操作
"""

import unittest
import requests
import json
import time
import subprocess
import os
import signal

class PPDBHttpTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """启动 PPDB 服务器"""
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
        """停止 PPDB 服务器"""
        cls.server_process.send_signal(signal.SIGTERM)
        cls.server_process.wait()
        os.system(f"rm -rf {cls.data_dir}")
    
    def test_put(self):
        """测试 PUT 操作"""
        url = f"{self.base_url}/kv/test_key"
        data = {"value": "test_value"}
        response = requests.put(url, json=data)
        self.assertEqual(response.status_code, 200)
    
    def test_get(self):
        """测试 GET 操作"""
        # 先写入数据
        key = "get_test_key"
        value = "get_test_value"
        put_url = f"{self.base_url}/kv/{key}"
        requests.put(put_url, json={"value": value})
        
        # 测试获取
        get_url = f"{self.base_url}/kv/{key}"
        response = requests.get(get_url)
        self.assertEqual(response.status_code, 200)
        self.assertEqual(response.json()["value"], value)
    
    def test_delete(self):
        """测试 DELETE 操作"""
        # 先写入数据
        key = "delete_test_key"
        put_url = f"{self.base_url}/kv/{key}"
        requests.put(put_url, json={"value": "to_be_deleted"})
        
        # 删除数据
        delete_url = f"{self.base_url}/kv/{key}"
        response = requests.delete(delete_url)
        self.assertEqual(response.status_code, 200)
        
        # 验证删除
        get_response = requests.get(delete_url)
        self.assertEqual(get_response.status_code, 404)
    
    def test_nonexistent_key(self):
        """测试访问不存在的键"""
        url = f"{self.base_url}/kv/nonexistent_key"
        response = requests.get(url)
        self.assertEqual(response.status_code, 404)

if __name__ == '__main__':
    unittest.main()

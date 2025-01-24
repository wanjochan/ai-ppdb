import unittest
import time
from pymemcache.client.base import Client

class TestMemKVProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # 连接到已运行的服务
        print("Connecting to memkv service on localhost:11211")
        cls.client = Client(('localhost', 11211))

    @classmethod
    def tearDownClass(cls):
        cls.client.close()

    def setUp(self):
        # 每个测试前清空数据
        self.client.flush_all()

    def test_basic_set_get(self):
        # 测试基本的 set/get 操作
        print("Testing basic set/get...")
        self.assertTrue(self.client.set('key1', 'value1'))
        self.assertEqual(self.client.get('key1'), b'value1')

    def test_multi_set_get(self):
        # 测试多个键值对
        print("Testing multiple set/get...")
        test_data = {
            'key1': 'value1',
            'key2': 'value2',
            'key3': 'value3'
        }
        for k, v in test_data.items():
            self.assertTrue(self.client.set(k, v))
        
        for k, v in test_data.items():
            self.assertEqual(self.client.get(k), v.encode())

    def test_delete(self):
        # 测试删除操作
        print("Testing delete...")
        self.client.set('key1', 'value1')
        self.assertTrue(self.client.delete('key1'))
        self.assertIsNone(self.client.get('key1'))

    def test_expiration(self):
        # 测试过期时间
        print("Testing expiration...")
        self.client.set('key1', 'value1', expire=1)
        self.assertEqual(self.client.get('key1'), b'value1')
        time.sleep(2)
        self.assertIsNone(self.client.get('key1'))

    def test_increment_decrement(self):
        # 测试自增自减
        print("Testing increment/decrement...")
        self.client.set('counter', '0')
        self.assertEqual(self.client.incr('counter', 1), 1)
        self.assertEqual(self.client.incr('counter', 2), 3)
        self.assertEqual(self.client.decr('counter', 1), 2)

    def test_large_values(self):
        # 测试大数据
        print("Testing large values...")
        large_value = 'x' * 1024 * 1024  # 1MB
        self.assertTrue(self.client.set('large_key', large_value))
        self.assertEqual(self.client.get('large_key'), large_value.encode())

if __name__ == '__main__':
    unittest.main()

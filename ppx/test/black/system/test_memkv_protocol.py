import unittest
import time
import logging
from pymemcache.client.base import Client

# 配置日志
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

class TestMemKVProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # 连接到已运行的服务
        logging.info("Connecting to memkv service on localhost:11211")
        cls.client = Client(('localhost', 11211))
        logging.info("Connection established")

    @classmethod
    def tearDownClass(cls):
        logging.info("Closing connection")
        cls.client.close()

    def setUp(self):
        # 每个测试前清空数据
        logging.info("Setting up test - flushing all data")
        self.client.flush_all()

    def test_basic_set_get(self):
        # 测试基本的 set/get 操作
        logging.info("Testing basic set/get...")
        key, value = 'key1', 'value1'
        
        logging.debug(f"Setting key='{key}' value='{value}'")
        result = self.client.set(key, value)
        logging.debug(f"Set result: {result}")
        
        logging.debug(f"Getting key='{key}'")
        got_value = self.client.get(key)
        logging.debug(f"Got value: {got_value}")
        
        self.assertTrue(result)
        self.assertEqual(got_value, b'value1')

    def test_multi_set_get(self):
        # 测试多个键值对
        logging.info("Testing multiple set/get...")
        test_data = {
            'key1': 'value1',
            'key2': 'value2',
            'key3': 'value3'
        }
        for k, v in test_data.items():
            logging.debug(f"Setting key='{k}' value='{v}'")
            result = self.client.set(k, v)
            logging.debug(f"Set result: {result}")
            self.assertTrue(result)
        
        for k, v in test_data.items():
            logging.debug(f"Getting key='{k}'")
            got_value = self.client.get(k)
            logging.debug(f"Got value: {got_value}")
            self.assertEqual(got_value, v.encode())

    def test_delete(self):
        # 测试删除操作
        logging.info("Testing delete...")
        key, value = 'key1', 'value1'
        
        logging.debug(f"Setting key='{key}' value='{value}'")
        self.client.set(key, value)
        
        logging.debug(f"Deleting key='{key}'")
        result = self.client.delete(key)
        logging.debug(f"Delete result: {result}")
        
        logging.debug(f"Getting deleted key='{key}'")
        got_value = self.client.get(key)
        logging.debug(f"Got value after delete: {got_value}")
        
        self.assertTrue(result)
        self.assertIsNone(got_value)

    def test_expiration(self):
        # 测试过期时间
        logging.info("Testing expiration...")
        key, value = 'key1', 'value1'
        
        logging.debug(f"Setting key='{key}' value='{value}' with 1s expiration")
        self.client.set(key, value, expire=1)
        
        logging.debug(f"Getting key immediately")
        got_value = self.client.get(key)
        logging.debug(f"Got value: {got_value}")
        self.assertEqual(got_value, b'value1')
        
        logging.debug("Waiting for expiration (2s)")
        time.sleep(2)
        
        logging.debug(f"Getting key after expiration")
        got_value = self.client.get(key)
        logging.debug(f"Got value after expiration: {got_value}")
        self.assertIsNone(got_value)

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

#!/usr/bin/env python3
"""
Stress test for PPDB using memcache protocol
This script performs various stress tests against the PPDB server
using the memcache protocol on port 11211
"""

import memcache
import random
import string
import time
import threading
import argparse
from concurrent.futures import ThreadPoolExecutor
import sys
import logging
import socket

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(threadName)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class PPDBStressTest:
    def __init__(self, host='127.0.0.1', port=11211, num_threads=4):
        """Initialize stress test parameters"""
        self.host = host
        self.port = port
        self.num_threads = num_threads
        self.client = None
        self.stop_test = False
        self.stats = {
            'total_ops': 0,
            'successful_ops': 0,
            'failed_ops': 0,
            'total_time': 0,
            'avg_latency': 0,
            'set_errors': 0,
            'get_errors': 0,
            'delete_errors': 0,
            'connection_errors': 0
        }
        self._lock = threading.Lock()

    def _generate_random_string(self, length=10):
        """Generate random string for key/value"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

    def _update_stats(self, success, latency, error_type=None):
        """Update test statistics"""
        with self._lock:
            self.stats['total_ops'] += 1
            if success:
                self.stats['successful_ops'] += 1
            else:
                self.stats['failed_ops'] += 1
                if error_type:
                    self.stats[error_type] += 1
            self.stats['total_time'] += latency
            self.stats['avg_latency'] = self.stats['total_time'] / self.stats['total_ops']

    def _check_server(self):
        """Check if server is accessible"""
        try:
            sock = socket.create_connection((self.host, self.port), timeout=5)
            sock.close()
            return True
        except Exception as e:
            logger.error(f"Server connection check failed: {str(e)}")
            return False

    def _init_client(self):
        """Initialize memcache client with debug enabled"""
        try:
            self.client = memcache.Client(
                [f'{self.host}:{self.port}'],
                debug=True,  # Enable debug mode
                socket_timeout=5.0,  # Set socket timeout
                socket_connect_timeout=5.0  # Set connect timeout
            )
            # Test connection with a simple set/get
            test_key = 'test_connection'
            test_value = 'test_value'
            if not self.client.set(test_key, test_value):
                raise Exception("Failed to set test value")
            if self.client.get(test_key) != test_value:
                raise Exception("Failed to get test value")
            self.client.delete(test_key)
            return True
        except Exception as e:
            logger.error(f"Client initialization failed: {str(e)}")
            return False

    def _worker(self, worker_id):
        """Worker function that performs operations"""
        ops = 0
        client = memcache.Client(
            [f'{self.host}:{self.port}'],
            debug=True,
            socket_timeout=5.0,
            socket_connect_timeout=5.0
        )
        
        while not self.stop_test:
            # Generate random key and value
            key = f'key_{worker_id}_{self._generate_random_string()}'
            value = self._generate_random_string(100)  # 100 byte value
            
            try:
                # SET operation
                start_time = time.time()
                success = client.set(key, value)
                latency = time.time() - start_time
                if not success:
                    logger.error(f"Worker {worker_id}: SET operation failed for key {key}")
                    self._update_stats(False, latency, 'set_errors')
                    continue
                self._update_stats(success, latency)
                
                # GET operation
                start_time = time.time()
                result = client.get(key)
                latency = time.time() - start_time
                if result is None:
                    logger.error(f"Worker {worker_id}: GET operation failed for key {key}")
                    self._update_stats(False, latency, 'get_errors')
                    continue
                success = result == value
                if not success:
                    logger.error(f"Worker {worker_id}: Value mismatch for key {key}")
                self._update_stats(success, latency)
                
                # DELETE operation
                start_time = time.time()
                success = client.delete(key)
                latency = time.time() - start_time
                if not success:
                    logger.error(f"Worker {worker_id}: DELETE operation failed for key {key}")
                    self._update_stats(False, latency, 'delete_errors')
                    continue
                self._update_stats(success, latency)
                
                ops += 3
                if ops % 100 == 0:
                    logger.info(f'Worker {worker_id} completed {ops} operations')
                
            except Exception as e:
                logger.error(f'Worker {worker_id} error: {str(e)}')
                self._update_stats(False, 0, 'connection_errors')
                time.sleep(1)  # Wait before retry

    def run_test(self, duration=60):
        """Run stress test for specified duration"""
        # Check server accessibility
        if not self._check_server():
            logger.error(f"Server {self.host}:{self.port} is not accessible")
            return False
            
        # Initialize test client
        if not self._init_client():
            logger.error("Failed to initialize test client")
            return False
            
        logger.info(f'Starting stress test with {self.num_threads} threads for {duration} seconds')
        
        # Create thread pool
        with ThreadPoolExecutor(max_workers=self.num_threads) as executor:
            # Start workers
            futures = []
            for i in range(self.num_threads):
                futures.append(executor.submit(self._worker, i))
            
            # Wait for specified duration
            time.sleep(duration)
            self.stop_test = True
            
            # Wait for all workers to complete
            for future in futures:
                try:
                    future.result()
                except Exception as e:
                    logger.error(f"Worker thread failed: {str(e)}")
        
        # Print results
        logger.info('\nTest Results:')
        logger.info(f'Total Operations: {self.stats["total_ops"]}')
        logger.info(f'Successful Operations: {self.stats["successful_ops"]}')
        logger.info(f'Failed Operations: {self.stats["failed_ops"]}')
        logger.info(f'SET Errors: {self.stats["set_errors"]}')
        logger.info(f'GET Errors: {self.stats["get_errors"]}')
        logger.info(f'DELETE Errors: {self.stats["delete_errors"]}')
        logger.info(f'Connection Errors: {self.stats["connection_errors"]}')
        logger.info(f'Average Latency: {self.stats["avg_latency"]*1000:.2f} ms')
        logger.info(f'Operations/Second: {self.stats["total_ops"]/duration:.2f}')
        logger.info(f'Success Rate: {(self.stats["successful_ops"]/self.stats["total_ops"])*100:.2f}%')
        
        return self.stats["successful_ops"] > 0

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description='PPDB Stress Test')
    parser.add_argument('--host', default='127.0.0.1', help='Server host')
    parser.add_argument('--port', type=int, default=11211, help='Server port')
    parser.add_argument('--threads', type=int, default=4, help='Number of threads')
    parser.add_argument('--duration', type=int, default=60, help='Test duration in seconds')
    args = parser.parse_args()

    try:
        test = PPDBStressTest(args.host, args.port, args.threads)
        if not test.run_test(args.duration):
            logger.error("Stress test failed - no successful operations")
            sys.exit(1)
    except KeyboardInterrupt:
        logger.info('\nTest interrupted by user')
        sys.exit(1)
    except Exception as e:
        logger.error(f'Test failed: {str(e)}')
        sys.exit(1)

if __name__ == '__main__':
    main() 
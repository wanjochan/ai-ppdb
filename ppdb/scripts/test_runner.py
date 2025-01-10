#!/usr/bin/env python3
"""
example
d:\miniconda3\python.exe ppdb/scripts/test_runner.py -t 5 -d ".\ppdb\scripts\build_test_infra.bat"
"""
import argparse
import subprocess
import sys
import os
import time
import threading
import queue
import select
import tempfile
from datetime import datetime

def run_warmup():
    """运行预热命令"""
    print("=== 运行预热命令 ===")
    process = subprocess.Popen(
        ".\ppdb\scripts\build_test42.bat",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True
    )
    stdout, stderr = process.communicate()
    print("预热输出:")
    print(stdout.decode('utf-8', errors='replace'))
    if stderr:
        print("预热错误:", file=sys.stderr)
        print(stderr.decode('utf-8', errors='replace'), file=sys.stderr)
    return process.returncode

def read_stream(stream, q):
    """读取流并放入队列"""
    try:
        for line in iter(stream.readline, b''):
            q.put(line.decode('utf-8', errors='replace'))
    except (IOError, ValueError):
        pass
    finally:
        stream.close()

def run_test(args):
    """运行测试程序并处理输出"""
    # 先运行预热命令
    if run_warmup() != 0:
        print("预热失败，停止测试", file=sys.stderr)
        return 1
        
    # 创建临时文件
    stdout_file = tempfile.NamedTemporaryFile(
        prefix=f'test_stdout_{datetime.now().strftime("%Y%m%d_%H%M%S")}_',
        suffix='.log',
        delete=False,
        mode='wb'
    )
    stderr_file = tempfile.NamedTemporaryFile(
        prefix=f'test_stderr_{datetime.now().strftime("%Y%m%d_%H%M%S")}_',
        suffix='.log',
        delete=False,
        mode='wb'
    )
    
    print(f"输出重定向到:\nstdout: {stdout_file.name}\nstderr: {stderr_file.name}")
    
    try:
        # 启动进程
        process = subprocess.Popen(
            args.command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.PIPE if args.interactive else None,
            shell=True,
            bufsize=1,
            universal_newlines=False
        )
        
        # 创建队列存储输出
        stdout_q = queue.Queue()
        stderr_q = queue.Queue()
        
        # 创建线程读取输出
        stdout_t = threading.Thread(target=read_stream, args=(process.stdout, stdout_q))
        stderr_t = threading.Thread(target=read_stream, args=(process.stderr, stderr_q))
        stdout_t.daemon = True
        stderr_t.daemon = True
        stdout_t.start()
        stderr_t.start()
        
        # 等待超时或完成
        start_time = time.time()
        while True:
            # 检查是否超时
            if args.timeout and (time.time() - start_time) > args.timeout:
                process.kill()
                print(f"\n程序运行超时 ({args.timeout}秒)，已强制终止")
                break
                
            # 检查进程是否结束
            if process.poll() is not None:
                break
                
            # 处理输出
            try:
                while True:
                    try:
                        line = stdout_q.get_nowait()
                        print(line, end='', flush=True)
                        stdout_file.write(line.encode('utf-8'))
                        stdout_file.flush()
                    except queue.Empty:
                        break
                        
                while True:
                    try:
                        line = stderr_q.get_nowait()
                        print(line, end='', file=sys.stderr, flush=True)
                        stderr_file.write(line.encode('utf-8'))
                        stderr_file.flush()
                    except queue.Empty:
                        break
            except:
                if args.debug:
                    import traceback
                    traceback.print_exc()
                
            # 处理交互输入
            if args.interactive and sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                user_input = sys.stdin.readline()
                process.stdin.write(user_input.encode())
                process.stdin.flush()
                
            time.sleep(0.1)
        
        # 确保所有输出都被处理
        stdout_t.join(1)
        stderr_t.join(1)
        
        # 处理剩余输出
        while not stdout_q.empty():
            line = stdout_q.get()
            print(line, end='', flush=True)
            stdout_file.write(line.encode('utf-8'))
        while not stderr_q.empty():
            line = stderr_q.get()
            print(line, end='', file=sys.stderr, flush=True)
            stderr_file.write(line.encode('utf-8'))
        
        # 关闭文件
        stdout_file.close()
        stderr_file.close()
        
        # 打印日志内容
        print("\n=== stdout 日志内容 ===")
        with open(stdout_file.name, 'rb') as f:
            content = f.read()
            try:
                print(content.decode('utf-8'))
            except UnicodeDecodeError:
                print(content.decode('gbk', errors='replace'))
                
        print("\n=== stderr 日志内容 ===")
        with open(stderr_file.name, 'rb') as f:
            content = f.read()
            try:
                print(content.decode('utf-8'))
            except UnicodeDecodeError:
                print(content.decode('gbk', errors='replace'))
        
        return process.returncode
        
    finally:
        # 删除临时文件
        try:
            os.unlink(stdout_file.name)
            os.unlink(stderr_file.name)
        except:
            pass

def main():
    parser = argparse.ArgumentParser(description='智能测试运行器')
    parser.add_argument('command', help='要运行的命令')
    parser.add_argument('-t', '--timeout', type=float, help='超时时间（秒）')
    parser.add_argument('-i', '--interactive', action='store_true', help='是否需要交互')
    parser.add_argument('-d', '--debug', action='store_true', help='是否打印调试信息')
    parser.add_argument('-n', '--no-warmup', action='store_true', help='是否跳过预热')
    
    args = parser.parse_args()
    
    try:
        exit_code = run_test(args)
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n用户中断测试")
        sys.exit(1)
    except Exception as e:
        print(f"\n运行测试时发生错误: {e}")
        if args.debug:
            import traceback
            traceback.print_exc()
        sys.exit(2)

if __name__ == '__main__':
    main() 
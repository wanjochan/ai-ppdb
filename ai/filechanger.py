#!/usr/bin/env python3
"""Enhanced File Line Replacement Tool

A tool for replacing lines in files, supporting:
1. Single line operations with start,end numbers
2. Batch operations with format: 'start,end:content'
3. Line deletion with empty content
4. Pipe input support
5. UTF-8 encoding
"""

def replace_lines(filename, start, end, content=""):
    """替换文件中的指定行范围为新内容
    
    Args:
        filename: 要处理的文件路径
        start: 起始行号(支持负数,如 -1 表示最后一行)
        end: 结束行号(支持负数)
        content: 新的内容(默认为空字符串,表示删除这些行)
    """
    # 确保以 UTF-8 编码读取文件
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    total = len(lines)
    
    # 处理负数行号
    start = start if start > 0 else total + start + 1
    end = end if end > 0 else total + end + 1
    
    # 替换指定范围的行
    if isinstance(content, str):
        content = content.splitlines(True)
    lines[start-1:end] = content
    
    # 以 UTF-8 编码写回文件
    with open(filename, 'w', encoding='utf-8', newline='') as f:
        f.writelines(lines)

def process_batch_input(filename, batch_content):
    """处理批量替换输入
    
    输入格式：
    start,end:content
    start,end:content
    ...
    
    特殊字符处理：
    - \n   表示换行
    - \\n  表示字面的 \n
    - \\   表示字面的 \
    
    Args:
        filename: 要处理的文件路径
        batch_content: 批处理内容字符串
    """
    # 读取文件
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    total = len(lines)
    
    # 处理每一行批处理命令
    for cmd in batch_content.splitlines():
        if not cmd.strip() or not ':' in cmd:
            continue
            
        # 分割行号和内容
        range_part, content = cmd.split(':', 1)
        if ',' not in range_part:
            continue
            
        start, end = map(str.strip, range_part.split(','))
        try:
            start = int(start)
            end = int(end)
        except ValueError:
            continue
            
        # 处理负数行号
        start = start if start > 0 else total + start + 1
        end = end if end > 0 else total + end + 1
        
        # 处理转义字符
        # 1. 先将 \\ 转换为临时标记
        # 2. 将 \n 转换为实际的换行符
        # 3. 将临时标记转换回 \
        content = content.replace('\\\\', '\x00')  # 临时标记
        content = content.replace('\\n', '\n')
        content = content.replace('\x00', '\\')
        
        # 替换内容
        new_content = content.splitlines(True)
        lines[start-1:end] = new_content
    
    # 写回文件
    with open(filename, 'w', encoding='utf-8', newline='') as f:
        f.writelines(lines)

def test_replace_lines():
    """测试行替换功能"""
    test_file = "test.txt"
    with open(test_file, "w", encoding="utf-8") as f:
        f.write("line1\nline2\nline3\nline4\nline5\n")
    
    try:
        # 测试批处理替换
        batch_content = "1,2:new line1\nnew line2\n3,4:new line3\nnew line4"
        process_batch_input(test_file, batch_content)
        print("1. Testing batch replacement...", file=sys.stderr)
        
        # 验证结果
        with open(test_file, "r", encoding="utf-8") as f:
            content = f.read()
            print("\nFinal file content:", file=sys.stderr)
            print(content, file=sys.stderr)
            print("Test completed!", file=sys.stderr)
    finally:
        # 清理测试文件
        import os
        if os.path.exists(test_file):
            os.remove(test_file)

if __name__ == '__main__':
    import sys
    import os
    
    # 无参数时运行测试
    if len(sys.argv) == 1:
        print("Running tests...", file=sys.stderr)
        test_replace_lines()
    # 参数不足时显示用法
    elif len(sys.argv) < 2:
        print("Usage:", file=sys.stderr)
        print("  1. Single replacement:", file=sys.stderr)
        print("     python filechanger.py <file> <start> <end> [content]", file=sys.stderr)
        print("  2. Batch replacement:", file=sys.stderr)
        print("     echo 'start,end:content' | python filechanger.py <file>", file=sys.stderr)
        print("\nExamples:", file=sys.stderr)
        print("  Single:", file=sys.stderr)
        print("    python filechanger.py test.txt 1 3 'new content'", file=sys.stderr)
        print("  Batch:", file=sys.stderr)
        print("    echo '1,3:line1\\nline2' | python filechanger.py test.txt", file=sys.stderr)
        print("    echo '1,2:a\\\\n' | python filechanger.py test.txt  # 写入字面的 \\n", file=sys.stderr)
        sys.exit(1)
    # 执行替换
    else:
        filename = sys.argv[1]
        
        # 检查是否有管道输入
        if not sys.stdin.isatty():
            # 从管道读取内容，确保使用 UTF-8 编码
            if hasattr(sys.stdin, 'buffer'):
                import io
                content = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8').read()
            else:
                content = sys.stdin.read()
                
            # 如果输入包含 ':' 则视为批处理模式
            if ':' in content:
                try:
                    process_batch_input(filename, content)
                except Exception as e:
                    print(f"Error in batch processing: {str(e)}", file=sys.stderr)
                    sys.exit(1)
            else:
                # 单行模式需要完整参数
                if len(sys.argv) < 4:
                    print("Error: Single replacement mode requires <start> and <end> parameters", file=sys.stderr)
                    sys.exit(1)
                try:
                    start = int(sys.argv[2])
                    end = int(sys.argv[3])
                    replace_lines(filename, start, end, content)
                except Exception as e:
                    print(f"Error: {str(e)}", file=sys.stderr)
                    sys.exit(1)
        else:
            # 命令行模式需要完整参数
            if len(sys.argv) < 4:
                print("Error: Command line mode requires <start> and <end> parameters", file=sys.stderr)
                sys.exit(1)
            try:
                start = int(sys.argv[2])
                end = int(sys.argv[3])
                content = sys.argv[4] if len(sys.argv) > 4 else ""
                replace_lines(filename, start, end, content)
            except Exception as e:
                print(f"Error: {str(e)}", file=sys.stderr)
                sys.exit(1)

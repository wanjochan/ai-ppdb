#!/usr/bin/env python3
"""Enhanced File Line Replacement Tool

A tool designed for AI agents to modify files, particularly useful in Windows/PowerShell
environments. This tool replaces the default file modification methods available to agents.

Format for AI agents (PowerShell):
    @'
    start,end:content\\n
    start,end:content\\n
    '@ | python filechanger.py file.txt

    Or for multiple commands:
    @(
        'start,end:content\\n',
        'start,end:content\\n'
    ) | python filechanger.py file.txt

Key Features for AI Agents:
1. Supports both single and batch line replacements
2. Handles line numbers intuitively (-1 means last line)
3. Automatically manages newlines
4. Works well with PowerShell escape sequences

Example Usage in AI Agent Context:
1. Replace specific lines:
    @'
    1,2:def my_function():\\n    return True
    '@ | python filechanger.py file.py

2. Multiple operations:
    @(
        '1,1:# File: example.py\\n# Updated: 2024',
        '5,6:def new_function():\\n    print("Hello")',
        '-1,0:# End of file'
    ) | python filechanger.py file.py

3. Common operations:
    - Delete lines: '3,4:'
    - Insert at start: '0,0:new first line'
    - Append at end: '-1,0:new last line'
    - Replace single line: '5,5:new content'

Note:
    - Line numbers start from 1
    - Use 0,0 to insert at the beginning
    - Use -1,0 to append at the end
    - Empty content means delete the lines
    - Content will automatically end with newline
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
    
    # 特殊情况：插入到文件开头
    if start == 0 and end == 0:
        start = end = 1
    
    # 特殊情况：追加到文件末尾
    if start == total + 1:
        lines.append('')  # 确保有一个位置可以追加
    
    # 处理内容
    if isinstance(content, str):
        # 确保内容以换行符结尾
        if content and not content.endswith('\n'):
            content = content + '\n'
        content = content.splitlines(True)
    
    # 替换指定范围的行
    lines[start-1:end] = content
    
    # 确保文件以换行符结尾
    if lines and not lines[-1].endswith('\n'):
        lines[-1] = lines[-1] + '\n'
    
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
    - \\n   表示换行
    - \\\\n  表示字面的 \\n
    - \\\\   表示字面的 \\
    
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
        cmd = cmd.strip()
        if not cmd or not ':' in cmd:
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
            
        # 处理转义序列
        content = content.encode('raw_unicode_escape').decode('unicode_escape')
        
        # 执行替换
        try:
            replace_lines(filename, start, end, content)
        except Exception as e:
            print(f"Error processing command '{cmd}': {str(e)}", file=sys.stderr)

def test_replace_lines():
    """测试行替换功能"""
    test_file = "test.txt"
    
    # 准备测试文件
    with open(test_file, "w", encoding="utf-8") as f:
        f.write("line1\nline2\nline3\nline4\nline5\n")
    
    try:
        print("\nTest 1: Basic line replacement", file=sys.stderr)
        replace_lines(test_file, 2, 3, "new line2\nnew line3\n")
        with open(test_file, "r", encoding="utf-8") as f:
            print(f.read(), file=sys.stderr)
        
        print("\nTest 2: Delete lines", file=sys.stderr)
        replace_lines(test_file, 4, 5, "")
        with open(test_file, "r", encoding="utf-8") as f:
            print(f.read(), file=sys.stderr)
        
        print("\nTest 3: Insert at beginning", file=sys.stderr)
        replace_lines(test_file, 0, 0, "new first line\n")
        with open(test_file, "r", encoding="utf-8") as f:
            print(f.read(), file=sys.stderr)
        
        print("\nTest 4: Append at end", file=sys.stderr)
        replace_lines(test_file, -1, 0, "new last line\n")
        with open(test_file, "r", encoding="utf-8") as f:
            print(f.read(), file=sys.stderr)
        
        print("\nTest 5: Batch processing", file=sys.stderr)
        # 注意这里使用原始字符串来避免转义问题
        batch_content = r"""1,2:header line 1\nheader line 2
3,4:middle\ncontent
-1,0:last line 1\nlast line 2"""
        process_batch_input(test_file, batch_content)
        with open(test_file, "r", encoding="utf-8") as f:
            print(f.read(), file=sys.stderr)
            
    finally:
        # 清理测试文件
        import os
        if os.path.exists(test_file):
            os.remove(test_file)

def show_examples():
    """显示使用示例"""
    print("Examples:", file=sys.stderr)
    print("\n1. Single line replacement:", file=sys.stderr)
    print('   echo "new content" | python filechanger.py file.txt 1 1', file=sys.stderr)
    
    print("\n2. Replace multiple lines:", file=sys.stderr)
    print('   echo "line1\\nline2" | python filechanger.py file.txt 1 2', file=sys.stderr)
    
    print("\n3. Delete lines:", file=sys.stderr)
    print('   python filechanger.py file.txt 3 4', file=sys.stderr)
    
    print("\n4. Insert at beginning:", file=sys.stderr)
    print('   echo "new first line" | python filechanger.py file.txt 0 0', file=sys.stderr)
    
    print("\n5. Append at end:", file=sys.stderr)
    print('   echo "new last line" | python filechanger.py file.txt -1 0', file=sys.stderr)
    
    print("\n6. Batch processing:", file=sys.stderr)
    print('   Multiple operations in one command:', file=sys.stderr)
    print('   (echo 1,2:header line 1\\nheader line 2', file=sys.stderr)
    print('    echo 3,4:middle content', file=sys.stderr)
    print('    echo -1,0:last line) | python filechanger.py file.txt', file=sys.stderr)

if __name__ == '__main__':
    import sys
    import os
    
    # 无参数时运行测试
    if len(sys.argv) == 1:
        print("Running tests...", file=sys.stderr)
        test_replace_lines()
    # --help 或参数不足时显示用法
    elif len(sys.argv) < 2 or sys.argv[1] == '--help':
        print("Usage:", file=sys.stderr)
        print("  1. Single replacement:", file=sys.stderr)
        print("     python filechanger.py <file> <start> <end> [content]", file=sys.stderr)
        print("  2. Batch replacement:", file=sys.stderr)
        print("     echo 'start,end:content' | python filechanger.py <file>", file=sys.stderr)
        print("\nFor detailed examples, run:", file=sys.stderr)
        print("  python filechanger.py --examples", file=sys.stderr)
        sys.exit(1)
    # 显示详细示例
    elif sys.argv[1] == '--examples':
        show_examples()
        sys.exit(0)
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

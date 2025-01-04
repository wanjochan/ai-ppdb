#!/usr/bin/env python3
"""文件行替换工具

这个工具提供了一个简单的方法来替换文件中的指定行。支持:
1. 使用正数行号从文件开始处理
2. 使用负数行号从文件末尾处理
3. 通过提供空内容来删除行
4. 支持从管道读取内容
5. 自动处理UTF-8编码
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

def test_replace_lines():
    """测试行替换功能"""
    test_file = "test.txt"
    with open(test_file, "w", encoding="utf-8") as f:
        f.write("line1\nline2\nline3\nline4\nline5\n")
    
    try:
        # 测试正数行号替换
        replace_lines(test_file, 2, 3, "new line2\nnew line3\n")
        print("1. Testing positive line numbers...", file=sys.stderr)
        
        # 测试负数行号替换
        replace_lines(test_file, -2, -1, "new line4\nnew line5\n")
        print("2. Testing negative line numbers...", file=sys.stderr)
        
        # 测试删除行
        replace_lines(test_file, 1, 1, "")
        print("3. Testing line deletion...", file=sys.stderr)
        
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
    elif len(sys.argv) < 4:
        print("Usage: python filechanger.py <file> <start> <end> [content]", file=sys.stderr)
        print("Arguments:", file=sys.stderr)
        print("  file    - File path to modify", file=sys.stderr)
        print("  start   - Starting line number (supports negative numbers)", file=sys.stderr)
        print("  end     - Ending line number (supports negative numbers)", file=sys.stderr)
        print("  content - New content (optional, default is empty string to delete lines)", file=sys.stderr)
        print("\nExamples:", file=sys.stderr)
        print("  python filechanger.py test.txt 1 3 'new content'  # Replace lines 1-3", file=sys.stderr)
        print("  python filechanger.py test.txt -2 -1 'new end'    # Replace last two lines", file=sys.stderr)
        print("  python filechanger.py test.txt 5 7                 # Delete lines 5-7", file=sys.stderr)
        print("  echo 'new content' | python filechanger.py test.txt 1 3  # Read content from pipe", file=sys.stderr)
        sys.exit(1)
    # 执行替换
    else:
        filename = sys.argv[1]
        start = int(sys.argv[2])
        end = int(sys.argv[3])
        
        # 如果有第4个参数，使用它作为内容
        if len(sys.argv) > 4:
            content = sys.argv[4]
        # 否则检查是否有管道输入
        elif not sys.stdin.isatty():
            # 从管道读取内容，确保使用 UTF-8 编码
            if hasattr(sys.stdin, 'buffer'):
                import io
                content = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8').read()
            else:
                content = sys.stdin.read()
        else:
            content = ""
            
        try:
            replace_lines(filename, start, end, content)
        except Exception as e:
            print(f"Error: {str(e)}", file=sys.stderr)
            sys.exit(1)

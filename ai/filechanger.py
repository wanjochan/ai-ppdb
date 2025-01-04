#!/usr/bin/env python3
"""
简单的文件行替换工具

这个工具提供了一个简单的方法来替换文件中的指定行。支持:
1. 使用正数行号从文件开始处理
2. 使用负数行号从文件末尾处理
3. 通过提供空内容来删除行
"""

def replace_lines(filename, start, end, content=""):
    """替换文件中的指定行范围为新内容
    
    Args:
        filename: 要处理的文件路径
        start: 起始行号(支持负数,如 -1 表示最后一行)
        end: 结束行号(支持负数)
        content: 新的内容(默认为空字符串,表示删除这些行)
    """
    with open(filename, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    total = len(lines)
    
    # 处理负数行号
    start = start if start > 0 else total + start + 1
    end = end if end > 0 else total + end + 1
    
    # 替换指定范围的行
    lines[start-1:end] = content.splitlines(True) if content else []
    
    # 写回文件
    with open(filename, 'w', encoding='utf-8') as f:
        f.writelines(lines)

def test_replace_lines():
    """测试行替换功能"""
    test_file = "test.txt"
    with open(test_file, "w", encoding="utf-8") as f:
        f.write("line1\nline2\nline3\nline4\nline5\n")
    
    try:
        # 测试正数行号替换
        replace_lines(test_file, 2, 3, "new line2\nnew line3\n")
        print("1. 测试正数行号替换...")
        
        # 测试负数行号替换
        replace_lines(test_file, -2, -1, "new line4\nnew line5\n")
        print("2. 测试负数行号替换...")
        
        # 测试删除行
        replace_lines(test_file, 1, 1, "")
        print("3. 测试删除行...")
        
        # 验证结果
        with open(test_file, "r", encoding="utf-8") as f:
            content = f.read()
            print("\n最终文件内容:")
            print(content)
            print("测试完成!")
    finally:
        # 清理测试文件
        import os
        if os.path.exists(test_file):
            os.remove(test_file)

if __name__ == '__main__':
    import sys
    
    # 无参数时运行测试
    if len(sys.argv) == 1:
        print("运行测试...")
        test_replace_lines()
    # 参数不足时显示用法
    elif len(sys.argv) < 4:
        print("用法: python filechanger.py <file> <start> <end> [content]")
        print("参数:")
        print("  file    - 要修改的文件路径")
        print("  start   - 起始行号(支持负数)")
        print("  end     - 结束行号(支持负数)")
        print("  content - 新内容(可选,默认为空表示删除)")
        print("\n示例:")
        print("  python filechanger.py test.txt 1 3 'new content'  # 替换1-3行")
        print("  python filechanger.py test.txt -2 -1 'new end'    # 替换倒数第2行到最后")
        print("  python filechanger.py test.txt 5 7                 # 删除5-7行")
        sys.exit(1)
    # 执行替换
    else:
        filename = sys.argv[1]
        start = int(sys.argv[2])
        end = int(sys.argv[3])
        content = sys.argv[4] if len(sys.argv) > 4 else ""
        replace_lines(filename, start, end, content)

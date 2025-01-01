#!/bin/bash

# 设置路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."
ROOT_DIR="$(pwd)"

echo "=== PPDB 环境初始化脚本 ==="
echo

# 创建必要的目录
echo "创建目录结构..."
mkdir -p repos
mkdir -p tools/{cosmocc,cross9,cosmopolitan}
mkdir -p build

# 下载并安装工具链
echo
echo "下载工具链..."

# 下载并安装 cosmocc
if [ ! -d "tools/cosmocc/bin" ]; then
    echo "下载 cosmocc..."
    curl -L https://cosmo.zip/pub/cosmocc/cosmocc.zip -o cosmocc.zip
    echo "解压 cosmocc..."
    unzip -q cosmocc.zip -d tools/cosmocc
    echo "复制运行时文件..."
    cp tools/cosmocc/lib/cosmo/cosmopolitan.* tools/cosmopolitan/
    cp tools/cosmocc/lib/cosmo/ape.* tools/cosmopolitan/
    cp tools/cosmocc/lib/cosmo/crt.* tools/cosmopolitan/
    rm -f cosmocc.zip
else
    echo "cosmocc 已存在，跳过"
fi

# 下载并安装 cross9
if [ ! -d "tools/cross9/bin" ]; then
    echo "下载 cross9..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        curl -L https://github.com/jart/cosmopolitan/releases/download/3.2.1/cross9.zip -o cross9.zip
        unzip -q cross9.zip -d tools/cross9
        rm -f cross9.zip
    else
        # Linux
        curl -L https://github.com/jart/cosmopolitan/releases/download/3.2.1/cross9.tar.gz -o cross9.tar.gz
        tar xf cross9.tar.gz -C tools/cross9
        rm -f cross9.tar.gz
    fi
else
    echo "cross9 已存在，跳过"
fi

# 克隆参考代码
echo
echo "克隆参考代码..."
cd repos

if [ ! -d "leveldb" ]; then
    echo "克隆 leveldb..."
    git clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git
else
    echo "leveldb 已存在，跳过"
fi

cd ..

# 复制运行时文件到构建目录
echo
echo "准备构建目录..."
cp -f tools/cosmopolitan/ape.lds build/
cp -f tools/cosmopolitan/crt.o build/
cp -f tools/cosmopolitan/ape.o build/
cp -f tools/cosmopolitan/cosmopolitan.a build/

# 验证环境
echo
echo "验证环境..."

# 检查工具链
if [ ! -x "tools/cosmocc/bin/cosmocc" ]; then
    echo "错误：cosmocc 未正确安装"
    exit 1
fi

if [ ! -x "tools/cross9/bin/x86_64-pc-linux-gnu-gcc" ]; then
    echo "错误：cross9 未正确安装"
    exit 1
fi

# 检查运行时文件
if [ ! -f "tools/cosmopolitan/cosmopolitan.h" ]; then
    echo "错误：cosmopolitan 运行时文件未正确安装"
    exit 1
fi

# 验证编译器
echo "int main() { return 0; }" > test.c
tools/cosmocc/bin/cosmocc test.c -o test.com
if [ $? -ne 0 ]; then
    echo "错误：编译测试失败"
    rm -f test.c
    exit 1
fi
rm -f test.c test.com

echo
echo "=== 环境初始化完成 ==="
echo "你现在可以开始构建 PPDB 了"
echo "运行 'scripts/build.sh help' 查看构建选项"

exit 0 
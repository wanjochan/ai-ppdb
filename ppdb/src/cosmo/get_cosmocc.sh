#!/bin/bash

# 设置代理（如果需要）
export http_proxy=http://127.0.0.1:8888
export https_proxy=http://127.0.0.1:8888

# 创建工作目录
WORK_DIR="$HOME/cosmocc"
mkdir -p "$WORK_DIR"
cd "$WORK_DIR"

# 克隆cosmopolitan仓库
if [ ! -d "cosmopolitan" ]; then
    git clone https://github.com/jart/cosmopolitan.git
    cd cosmopolitan
else
    cd cosmopolitan
    git pull
fi

# 编译cosmocc
make -j$(nproc) o//tool/build/cosmocc

# 创建符号链接到/usr/local/bin（需要sudo权限）
sudo ln -sf "$WORK_DIR/cosmopolitan/o/tool/build/cosmocc" /usr/local/bin/cosmocc

echo "cosmocc has been installed to /usr/local/bin/cosmocc" 
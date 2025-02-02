#!/bin/bash

# 加载环境变量
source "$(dirname "$0")/build_env.sh"

# 检查是否已安装
if [ -d "${LIBDILL_DIR}" ]; then
    echo "libdill already installed at ${LIBDILL_DIR}"
    exit 0
fi

# 创建目录
mkdir -p "${ROOT_DIR}/repos"

# 克隆 libdill
echo "Cloning libdill..."
git clone https://github.com/sustrik/libdill.git "${LIBDILL_DIR}"

# 进入目录
cd "${LIBDILL_DIR}" || exit 1

# 配置和编译
./autogen.sh
./configure --prefix="${LIBDILL_DIR}"
make -j$(nproc)
make install

echo "libdill installed successfully"

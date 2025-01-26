#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}"

# 设置条件编译选项
ENABLE_RINETD=1
ENABLE_MEMKV=1

# 添加条件编译宏定义
if [ "${ENABLE_RINETD}" = "1" ]; then
    CFLAGS="${CFLAGS} -DDEV_RINETD"
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    CFLAGS="${CFLAGS} -DDEV_MEMKV"
fi

# 添加源目录到包含路径
CFLAGS="${CFLAGS} -I${SRC_DIR} -I${TOOLCHAIN_DIR}/include"

# 构建 ppdb
echo remove "${BUILD_DIR}/ppdb_latest.exe"
rm -f "${BUILD_DIR}/ppdb_latest.exe"
echo "Building ppdb..."
set -x  # 启用调试输出

# 准备源文件列表
SOURCES=(
    "${SRC_DIR}/ppdb/ppdb.c"
    "${SRC_DIR}/internal/poly/poly_cmdline.c"
    "${SRC_DIR}/internal/poly/poly_hashtable.c"
    "${SRC_DIR}/internal/poly/poly_atomic.c"
    "${SRC_DIR}/internal/poly/poly_memkv.c"
    "${SRC_DIR}/internal/peer/peer_service.c"
    "${SRC_DIR}/internal/infra/infra_core.c"
    "${SRC_DIR}/internal/infra/infra_memory.c"
    "${SRC_DIR}/internal/infra/infra_error.c"
    "${SRC_DIR}/internal/infra/infra_net.c"
    "${SRC_DIR}/internal/infra/infra_platform.c"
    "${SRC_DIR}/internal/infra/infra_sync.c"
)

# 根据条件添加源文件
if [ "${ENABLE_RINETD}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_rinetd.c")
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_memkv.c")
fi

# 编译
"${CC}" ${CFLAGS} "${SOURCES[@]}" ${LDFLAGS} -o "${BUILD_DIR}/ppdb_latest.exe"
set +x  # 关闭调试输出

if [ $? -ne 0 ]; then
    echo "Error: Build failed !!!"
    exit 1
fi

# 检查可执行文件是否存在且有执行权限
if [ ! -x "${BUILD_DIR}/ppdb_latest.exe" ]; then
    echo "Error: Executable not found or not executable"
    exit 1
fi

# 运行可执行文件
echo "List and run ppdb_latest.exe"
"${BUILD_DIR}/ppdb_latest.exe"

echo "copy to ppdb/"
cp -v "${BUILD_DIR}/ppdb_latest.exe" "${PPDB_DIR}/ppdb_latest.exe"
ls -al "${PPDB_DIR}/ppdb_latest.exe"

exit 0 

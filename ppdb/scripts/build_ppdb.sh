#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}"

# 添加源目录到包含路径
CFLAGS="${CFLAGS} -I${SRC_DIR} -I${TOOLCHAIN_DIR}/include"

# 构建 ppdb
echo remove "${BUILD_DIR}/ppdb_latest.exe"
rm -f "${BUILD_DIR}/ppdb_latest.exe"
echo "Building ppdb..."
set -x  # 启用调试输出
"${CC}" ${CFLAGS} \
    "${SRC_DIR}/ppdb/ppdb.c" \
    "${SRC_DIR}/internal/poly/poly_cmdline.c" \
    "${SRC_DIR}/internal/poly/poly_hashtable.c" \
    "${SRC_DIR}/internal/poly_atomic/poly_atomic.c" \
    "${SRC_DIR}/internal/peer/peer_rinetd.c" \
    "${SRC_DIR}/internal/peer/peer_memkv.c" \
    "${SRC_DIR}/internal/peer/peer_memkv_cmd.c" \
    "${SRC_DIR}/internal/infra/infra_core.c" \
    "${SRC_DIR}/internal/infra/infra_memory.c" \
    "${SRC_DIR}/internal/infra/infra_error.c" \
    "${SRC_DIR}/internal/infra/infra_net.c" \
    "${SRC_DIR}/internal/infra/infra_platform.c" \
    "${SRC_DIR}/internal/infra/infra_sync.c" \
    ${LDFLAGS} -o "${BUILD_DIR}/ppdb_latest.exe"
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

#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}"

# 添加源目录到包含路径
CFLAGS="${CFLAGS} -I${SRC_DIR} -I${COSMO}"

# 构建 ppdb
echo "Building ppdb..."
"${GCC}" ${CFLAGS} \
    "${SRC_DIR}/ppdb/ppdb.c" \
    "${SRC_DIR}/internal/poly/poly_cmdline.c" \
    "${SRC_DIR}/internal/peer/peer_rinetd.c" \
    "${SRC_DIR}/internal/infra/infra_core.c" \
    "${SRC_DIR}/internal/infra/infra_memory.c" \
    "${SRC_DIR}/internal/infra/infra_error.c" \
    "${SRC_DIR}/internal/infra/infra_mux.c" \
    "${SRC_DIR}/internal/infra/infra_mux_epoll.c" \
    "${SRC_DIR}/internal/infra/infra_net.c" \
    "${SRC_DIR}/internal/infra/infra_platform.c" \
    "${SRC_DIR}/internal/infra/infra_sync.c" \
    ${LDFLAGS} -o "${BUILD_DIR}/ppdb.dbg" \
    -Wl,-T,"${COSMO}/ape.lds" "${COSMO}/ape.o" "${COSMO}/crt.o" "${COSMO}/cosmopolitan.a"

if [ $? -ne 0 ]; then
    exit 1
fi

"${OBJCOPY}" -S -O binary "${BUILD_DIR}/ppdb.dbg" "${BUILD_DIR}/ppdb"
if [ $? -ne 0 ]; then
    exit 1
fi

# 如果没有明确禁用运行，则运行程序
if [ "$1" != "norun" ]; then
    cp -f "${BUILD_DIR}/ppdb" "${PPDB_DIR}/ppdb_latest"
    "${PPDB_DIR}/ppdb_latest" help
fi

exit 0 
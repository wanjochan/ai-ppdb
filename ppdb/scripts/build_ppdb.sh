#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/obj"

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
CFLAGS="${CFLAGS} -I${SRC_DIR} -I${TOOLCHAIN_DIR}/include -I${PPDB_DIR}/src -I${PPDB_DIR}/include -I${PPDB_DIR}/vendor/sqlite3 -I${PPDB_DIR}/vendor/duckdb"

# 构建 ppdb
echo remove "${BUILD_DIR}/ppdb_latest.exe"
rm -f "${BUILD_DIR}/ppdb_latest.exe"
rm -f "${PPDB_DIR}/ppdb_latest.exe"
echo "Building ppdb..."
# 准备源文件列表
SOURCES=(
    "${SRC_DIR}/internal/infra/infra_core.c"
    "${SRC_DIR}/internal/infra/infra_memory.c"
    "${SRC_DIR}/internal/infra/infra_error.c"
    "${SRC_DIR}/internal/infra/infra_net.c"
    "${SRC_DIR}/internal/infra/infra_platform.c"
    "${SRC_DIR}/internal/infra/infra_sync.c"
    "${SRC_DIR}/internal/poly/poly_cmdline.c"
    "${SRC_DIR}/internal/poly/poly_atomic.c"
    "${SRC_DIR}/internal/poly/poly_plugin.c"
    "${SRC_DIR}/internal/poly/poly_memkv.c"
    "${SRC_DIR}/internal/peer/peer_service.c"
    "${SRC_DIR}/ppdb/ppdb.c"
)

# 根据条件添加源文件
if [ "${ENABLE_RINETD}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_rinetd.c")
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_memkv.c")
fi

# 获取 CPU 核心数
if [ "$(uname)" = "Darwin" ]; then
    JOBS=$(sysctl -n hw.ncpu)
else
    JOBS=$(nproc)
fi

# 编译第三方库(TODO call build_poly.sh instead)
echo "Building sqlite3..."
if [ ! -f "${BUILD_DIR}/obj/sqlite3.o" ] || [ "${PPDB_DIR}/vendor/sqlite3/sqlite3.c" -nt "${BUILD_DIR}/obj/sqlite3.o" ]; then
    "${CC}" ${CFLAGS} -c "${PPDB_DIR}/vendor/sqlite3/sqlite3.c" -o "${BUILD_DIR}/obj/sqlite3.o"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to build sqlite3"
        rm -f "${BUILD_DIR}/obj/sqlite3.o"
        exit 1
    fi
fi

# 增量编译源文件
echo "Building sources..."
OBJECTS=()
PIDS=()

compile_file() {
    local src=$1
    local obj=$2
    echo "Compiling ${src}..."
    "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
    local ret=$?
    if [ $ret -ne 0 ]; then
        echo "Error: Failed to compile ${src}"
        rm -f "${obj}"  # 删除可能生成的不完整目标文件
        # 向父进程发送信号，通知构建失败
        kill -TERM $PPID
        exit $ret
    fi
}

for src in "${SOURCES[@]}"; do
    obj="${BUILD_DIR}/obj/$(basename "${src}" .c).o"
    OBJECTS+=("${obj}")
    if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
        # 如果正在运行的任务数达到 CPU 核心数，等待一个任务完成
        while [ ${#PIDS[@]} -ge $JOBS ]; do
            for i in ${!PIDS[@]}; do
                if ! kill -0 ${PIDS[$i]} 2>/dev/null; then
                    # 检查编译任务的返回值
                    wait ${PIDS[$i]}
                    ret=$?
                    if [ $ret -ne 0 ]; then
                        echo "Error: A compilation task failed"
                        # 清理所有正在运行的任务
                        for pid in ${PIDS[@]}; do
                            kill $pid 2>/dev/null
                        done
                        exit $ret
                    fi
                    unset PIDS[$i]
                fi
            done
            PIDS=("${PIDS[@]}")  # 重新打包数组
            [ ${#PIDS[@]} -ge $JOBS ] && sleep 0.1
        done
        
        # 启动新的编译任务
        compile_file "${src}" "${obj}" &
        PIDS+=($!)
    fi
done

# 等待所有编译任务完成并检查返回值
for pid in ${PIDS[@]}; do
    wait $pid
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "Error: A compilation task failed"
        exit $ret
    fi
done

# 链接
echo "Linking..."
"${CC}" ${LDFLAGS} "${OBJECTS[@]}" "${BUILD_DIR}/obj/sqlite3.o" -o "${BUILD_DIR}/ppdb_latest.exe"
if [ $? -ne 0 ]; then
    echo "Error: Linking failed"
    rm -f "${BUILD_DIR}/ppdb_latest.exe"
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

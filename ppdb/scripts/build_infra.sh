#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    echo "Error: Failed to load build environment"
    exit 1
fi

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/infra"

# 编译所有 infra 源文件
echo -e "${GREEN}Building infra library...${NC}"

# 清理旧的库文件
rm -vf "${BUILD_DIR}/infra/libinfra.a"

# 定义源文件
INFRA_SOURCES=(
    "${SRC_DIR}/internal/infra/infra_core.c"
    "${SRC_DIR}/internal/infra/infra_memory.c"
    "${SRC_DIR}/internal/infra/infra_error.c"
    "${SRC_DIR}/internal/infra/infra_net.c"
    "${SRC_DIR}/internal/infra/infra_platform.c"
    "${SRC_DIR}/internal/infra/infra_sync.c"
    "${SRC_DIR}/internal/infra/infra_gc.c"
)

# TODO if FLAG_BUILD_CLEAN...
#rm -vf ${BUILD_DIR}/infra/*.o

# 设置最大并发数
MAX_JOBS=2
## 自动设置最大并发数为CPU核心数
#MAX_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 编译函数
compile_file() {
    local src=$1
    local obj="${BUILD_DIR}/infra/$(basename "${src}" .c).o"
    
    # 检查源文件是否存在
    if [ ! -f "$src" ]; then
        echo -e "${RED}Error: Source file ${src} not found${NC}"
        return 1
    fi

    # 获取头文件依赖
    local headers=$(${CC} -MM "${src}" -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -I"${TOOLCHAIN_DIR}/include" | sed 's/.*: //' | tr ' \\' '\n' | grep '\.h$')
    
    # 检查是否需要重新编译
    local need_compile=0
    if [ ! -f "$obj" ]; then
        need_compile=1
    else
        # 检查源文件时间戳
        if [ "$src" -nt "$obj" ]; then
            need_compile=1
        fi
        # 检查所有头文件的时间戳
        for header in $headers; do
            if [ -f "$header" ] && [ "$header" -nt "$obj" ]; then
                need_compile=1
                break
            fi
        done
    fi
    
    if [ $need_compile -eq 0 ]; then
        echo -e "${YELLOW}Skipping ${src} (up to date)${NC}"
        return 0
    fi
    
    echo -e "${GREEN}Compiling ${src}...${NC}"
    ${CC} ${CFLAGS} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -I"${TOOLCHAIN_DIR}/include" -c "${src}" -o "${obj}"
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Failed to compile ${src}${NC}"
        return 1
    fi
    return 0
}

# 并行编译所有源文件
echo "Starting parallel compilation (max ${MAX_JOBS} jobs)..."
pids=()
current_jobs=0

for src in "${INFRA_SOURCES[@]}"; do
    # 等待，直到有空闲的编译槽
    while [ ${current_jobs} -ge ${MAX_JOBS} ]; do
        for i in ${!pids[@]}; do
            if ! kill -0 ${pids[i]} 2>/dev/null; then
                unset pids[i]
                let current_jobs--
            fi
        done
        sleep 0.1
    done
    
    # 启动新的编译任务
    compile_file "$src" &
    pids+=($!)
    let current_jobs++
done

# 等待所有剩余的编译进程完成
failed=0
for pid in ${pids[@]}; do
    if [ ! -z "$pid" ]; then
        wait $pid || let "failed+=1"
    fi
done

if [ "$failed" -ne 0 ]; then
    echo -e "${RED}Error: $failed compilation(s) failed${NC}"
    exit 1
fi

# 创建静态库
echo -e "${GREEN}Creating static library...${NC}"
"${AR}" rcs "${BUILD_DIR}/infra/libinfra.a" "${BUILD_DIR}"/infra/*.o
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create static library${NC}"
    exit 1
fi

echo -e "${GREEN}Build complete.${NC}"
ls -lh "${BUILD_DIR}/infra/libinfra.a"

exit 0 

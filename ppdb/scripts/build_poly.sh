#!/bin/bash
# 记录开始时间
START_TIME=$(date +%s.%N)

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    echo "Error: Failed to load build environment"
    exit 1
fi

# 添加开始时间统计
start_time=$(date +%s)

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/poly"

# 编译所有 poly 源文件
echo -e "${GREEN}Building poly library...${NC}"

# 清理旧的库文件
rm -vf "${BUILD_DIR}/poly/libpoly.a"

# 定义源文件
SRC_FILES=(
    "${SRC_DIR}/internal/poly/poly_memkv.c"
    "${SRC_DIR}/internal/poly/poly_memkv_cmd.c"
    "${SRC_DIR}/internal/poly/poly_db.c"
    "${SRC_DIR}/internal/poly/poly_plugin.c"
)

# 设置最大并发数（根据CPU核心数）
MAX_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

# 编译函数
compile_file() {
    local src=$1
    local obj="${BUILD_DIR}/poly/$(basename "${src}" .c).o"
    
    # 检查是否需要重新编译
    if [ -f "$obj" ] && [ "$src" -ot "$obj" ]; then
        echo -e "${YELLOW}Skipping ${src} (up to date)${NC}"
        return 0
    fi
    
    echo -e "${GREEN}Compiling ${src}...${NC}"
    ${CC} ${CFLAGS} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" \
        -I"${PPDB_DIR}/vendor/sqlite3" -I"${PPDB_DIR}/vendor/duckdb" \
        -c "${src}" -o "${obj}"
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

for src in "${SRC_FILES[@]}"; do
    # 等待，直到有空闲的编译槽
    while [ ${current_jobs} -ge ${MAX_JOBS} ]; do
        for i in ${!pids[@]}; do
            if ! kill -0 ${pids[i]} 2>/dev/null; then
                wait ${pids[i]}
                if [ $? -ne 0 ]; then
                    echo -e "${RED}Error: Compilation failed${NC}"
                    exit 1
                fi
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
for pid in ${pids[@]}; do
    if [ ! -z "$pid" ]; then
        wait $pid
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error: Compilation failed${NC}"
            exit 1
        fi
    fi
done

# # 创建静态库（暂时不需要）
# echo -e "${GREEN}Creating static library...${NC}"
# "${AR}" rcs "${BUILD_DIR}/poly/libpoly.a" "${BUILD_DIR}"/poly/*.o
# if [ $? -ne 0 ]; then
#     echo -e "${RED}Error: Failed to create static library${NC}"
#     exit 1
# fi

# echo -e "${GREEN}Build complete.${NC}"
# ls -lh "${BUILD_DIR}/poly/libpoly.a"

# 在文件末尾添加编译时间统计
end_time=$(date +%s)
duration=$((end_time - start_time))
echo -e "${GREEN}Build completed in ${duration} seconds.${NC}"

exit 0

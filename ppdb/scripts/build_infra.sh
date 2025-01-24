#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 设置源文件列表
SRC_FILES="infra_core.c infra_platform.c infra_sync.c infra_error.c infra_ds.c infra_memory.c infra_net.c"

# 记录开始时间
START_TIME=$(date +%s.%N)

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/infra"

echo "Checking infra layer..."

# 检查是否需要重新构建
NEED_REBUILD=0
if [ ! -f "${BUILD_DIR}/infra/libinfra.a" ]; then
    NEED_REBUILD=1
fi

# 检查每个源文件是否需要重新构建
for src_file in ${SRC_FILES}; do
    SRC_PATH="${PPDB_DIR}/src/internal/infra/${src_file}"
    OBJ_PATH="${BUILD_DIR}/infra/${src_file%.c}.o"
    
    if [ ! -f "${OBJ_PATH}" ]; then
        NEED_REBUILD=1
        continue
    fi
    
    if [ "${SRC_PATH}" -nt "${OBJ_PATH}" ]; then
        NEED_REBUILD=1
    fi
done

if [ ${NEED_REBUILD} -eq 1 ]; then
    echo "Building infra layer..."
    
    # 构建每个模块
    for src_file in ${SRC_FILES}; do
        SRC_PATH="${PPDB_DIR}/src/internal/infra/${src_file}"
        OBJ_PATH="${BUILD_DIR}/infra/${src_file%.c}.o"
        
        NEED_BUILD=0
        if [ ! -f "${OBJ_PATH}" ]; then
            NEED_BUILD=1
        elif [ "${SRC_PATH}" -nt "${OBJ_PATH}" ]; then
            NEED_BUILD=1
        fi
        
        if [ ${NEED_BUILD} -eq 1 ]; then
            echo "Building ${src_file%.c}..."
            "${CC}" ${CFLAGS} -I"${COSMO}" -I"${SRC_DIR}" -I"${PPDB_DIR}/src" "${SRC_PATH}" -c -o "${OBJ_PATH}"
            if [ $? -ne 0 ]; then
                exit 1
            fi
        else
            echo "${src_file%.c} is up to date."
        fi
    done
    
    # 创建静态库
    echo "Creating library..."
    "${AR}" rcs "${BUILD_DIR}/infra/libinfra.a" \
        "${BUILD_DIR}/infra/infra_core.o" \
        "${BUILD_DIR}/infra/infra_platform.o" \
        "${BUILD_DIR}/infra/infra_sync.o" \
        "${BUILD_DIR}/infra/infra_error.o" \
        "${BUILD_DIR}/infra/infra_ds.o" \
        "${BUILD_DIR}/infra/infra_memory.o" \
        "${BUILD_DIR}/infra/infra_net.o"
    if [ $? -ne 0 ]; then
        exit 1
    fi
    
    echo "Build infra complete."
else
    echo "Infra layer is up to date."
fi

# 计算并显示构建时间
END_TIME=$(date +%s.%N)
DIFF=$(echo "$END_TIME - $START_TIME" | bc)
printf "Build time: %.3f seconds\n" $DIFF

exit 0 

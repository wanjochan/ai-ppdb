#!/bin/bash

# # 加载环境变量和通用函数
# source "$(dirname "$0")/build_env.sh"
# if [ $? -ne 0 ]; then
#     echo "Error: Failed to load build environment"
#     exit 1
# fi

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

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

# 编译源文件
compile_files "${INFRA_SOURCES[@]}" "${BUILD_DIR}/infra" "infra"

# 创建静态库
echo -e "${GREEN}Creating static library...${NC}"
"${AR}" rcs "${BUILD_DIR}/infra/libinfra.a" "${OBJECTS[@]}"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create static library${NC}"
    exit 1
fi

echo -e "${GREEN}Build complete.${NC}"
ls -lh "${BUILD_DIR}/infra/libinfra.a"

exit 0

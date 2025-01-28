#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    echo "Error: Failed to load build environment"
    exit 1
fi

# 检查 sqlite3.h 和 duckdb.h 是否存在
if [ ! -f "${PPDB_DIR}/vendor/sqlite3/sqlite3.h" ]; then
    echo -e "${RED}Error: sqlite3.h not found in ${PPDB_DIR}/vendor/sqlite3/${NC}"
    exit 1
fi

if [ ! -f "${PPDB_DIR}/vendor/duckdb/duckdb.h" ]; then
    echo -e "${RED}Error: duckdb.h not found in ${PPDB_DIR}/vendor/duckdb/${NC}" 
    exit 1
fi

# 创建构建目录
mkdir -p "${BUILD_DIR}/test/black/poly"
mkdir -p "${BUILD_DIR}/vendor/sqlite3"

# 编译 SQLite
echo -e "${GREEN}Building SQLite...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    "${PPDB_DIR}/vendor/sqlite3/sqlite3.c" \
    -c -o "${BUILD_DIR}/vendor/sqlite3/sqlite3.o"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to build SQLite${NC}"
    exit 1
fi

# 编译测试源文件
echo -e "${GREEN}Building poly tests...${NC}"

# 清理旧的测试文件
rm -f "${BUILD_DIR}/test/black/poly/test_poly_sqlite"
rm -f "${BUILD_DIR}/test/black/poly/test_poly_duckdb"

# Build SQLite implementation
echo -e "Building SQLite implementation..."
mkdir -p "${BUILD_DIR}/src/internal/poly"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${SRC_DIR}" \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    "${SRC_DIR}/internal/poly/poly_sqlite.c" \
    -c -o "${BUILD_DIR}/src/internal/poly/poly_sqlite.o"
if [ $? -ne 0 ]; then
    echo -e "Error: Failed to build SQLite implementation"
    exit 1
fi

# 编译测试框架
echo -e "${GREEN}Building test framework...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${SRC_DIR}" \
    "${PPDB_DIR}/test/white/framework/test_framework.c" \
    -c -o "${BUILD_DIR}/test/black/poly/test_framework.o"

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to build test framework${NC}"
    exit 1
fi

# 编译 SQLite 测试程序
echo -e "${GREEN}Building SQLite tests...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/test/black/poly" \
    -I"${SRC_DIR}" \
    "${PPDB_DIR}/test/black/poly/test_poly_sqlite.c" \
    "${BUILD_DIR}/test/black/poly/test_framework.o" \
    "${BUILD_DIR}/src/internal/poly/poly_sqlite.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    "${BUILD_DIR}/vendor/sqlite3/sqlite3.o" \
    -o "${BUILD_DIR}/test/black/poly/test_poly_sqlite"

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to build SQLite tests${NC}"
    exit 1
fi

# 编译 DuckDB 测试程序
# echo -e "${GREEN}Building DuckDB tests...${NC}"
# ${CC} ${CFLAGS} \
#     -I"${PPDB_DIR}" \
#     -I"${PPDB_DIR}/include" \
#     -I"${SRC_DIR}" \
#     -I"${PPDB_DIR}/vendor/sqlite3" \
#     -I"${PPDB_DIR}/vendor/duckdb" \
#     "${TEST_DIR}/black/poly/test_poly_duckdb.c" \
#     "${BUILD_DIR}/test/black/poly/test_framework.o" \
#     "${BUILD_DIR}/poly/poly_duckdb.o" \
#     "${BUILD_DIR}/infra/libinfra.a" \
#     -o "${BUILD_DIR}/test/black/poly/test_poly_duckdb"

# if [ $? -ne 0 ]; then
#     echo -e "${RED}Error: Failed to build DuckDB tests${NC}"
#     exit 1
# fi

echo -e "${GREEN}Build complete.${NC}"
ls -lh "${BUILD_DIR}/test/black/poly/test_poly_"*

# 运行测试
echo -e "${GREEN}Running SQLite tests...${NC}"
"${BUILD_DIR}/test/black/poly/test_poly_sqlite"

# echo -e "${GREEN}Running DuckDB tests...${NC}"
# "${BUILD_DIR}/test/black/poly/test_poly_duckdb"

exit $? 

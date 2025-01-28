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
mkdir -p "${BUILD_DIR}/vendor/duckdb"

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

# 复制 DuckDB 动态库
echo -e "${GREEN}Copying DuckDB library...${NC}"
cp "${PPDB_DIR}/vendor/duckdb/libduckdb.dylib" "${BUILD_DIR}/vendor/duckdb/"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to copy DuckDB library${NC}"
    exit 1
fi

# 编译测试源文件
echo -e "${GREEN}Building poly tests...${NC}"

# 清理旧的测试文件
rm -f "${BUILD_DIR}/test/black/poly/test_poly_sqlite"
rm -f "${BUILD_DIR}/test/black/poly/test_poly_duckdb"
rm -f "${BUILD_DIR}/test/black/poly/test_poly_memkv"

# 编译实现文件
echo -e "${GREEN}Building SQLite implementation...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    "${PPDB_DIR}/src/internal/poly/poly_sqlite.c" \
    -c -o "${BUILD_DIR}/test/black/poly/poly_sqlite.o"

echo -e "${GREEN}Building DuckDB implementation...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/duckdb" \
    "${PPDB_DIR}/src/internal/poly/poly_duckdb.c" \
    -c -o "${BUILD_DIR}/test/black/poly/poly_duckdb.o"

echo -e "${GREEN}Building MemKV implementation...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    "${PPDB_DIR}/src/internal/poly/poly_memkv.c" \
    -c -o "${BUILD_DIR}/test/black/poly/poly_memkv.o"

# 编译测试框架
echo -e "${GREEN}Building test framework...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    "${PPDB_DIR}/test/white/framework/test_framework.c" \
    -c -o "${BUILD_DIR}/test/black/poly/test_framework.o"

# 编译测试文件
echo -e "${GREEN}Building SQLite tests...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    "${PPDB_DIR}/test/black/poly/test_poly_sqlite.c" \
    "${BUILD_DIR}/test/black/poly/poly_sqlite.o" \
    "${BUILD_DIR}/test/black/poly/test_framework.o" \
    "${BUILD_DIR}/vendor/sqlite3/sqlite3.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    -o "${BUILD_DIR}/test/black/poly/test_poly_sqlite"

echo -e "${GREEN}Building DuckDB tests...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/duckdb" \
    "${PPDB_DIR}/test/black/poly/test_poly_duckdb.c" \
    "${BUILD_DIR}/test/black/poly/poly_duckdb.o" \
    "${BUILD_DIR}/test/black/poly/test_framework.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    -o "${BUILD_DIR}/test/black/poly/test_poly_duckdb"

echo -e "${GREEN}Building MemKV tests...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    "${PPDB_DIR}/test/black/poly/test_poly_memkv.c" \
    "${BUILD_DIR}/test/black/poly/poly_memkv.o" \
    "${BUILD_DIR}/test/black/poly/poly_sqlite.o" \
    "${BUILD_DIR}/test/black/poly/poly_duckdb.o" \
    "${BUILD_DIR}/test/black/poly/test_framework.o" \
    "${BUILD_DIR}/vendor/sqlite3/sqlite3.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    -o "${BUILD_DIR}/test/black/poly/test_poly_memkv"

echo -e "${GREEN}Build complete.${NC}"
ls -lh "${BUILD_DIR}/test/black/poly/test_poly_"*

# 运行测试
echo -e "${GREEN}Running SQLite tests...${NC}"
"${BUILD_DIR}/test/black/poly/test_poly_sqlite"

echo -e "${GREEN}Running DuckDB tests...${NC}"
# 设置 DuckDB 库路径
export DYLD_LIBRARY_PATH="${BUILD_DIR}/vendor/duckdb:${DYLD_LIBRARY_PATH}"
export DUCKDB_LIBRARY_PATH="${BUILD_DIR}/vendor/duckdb/libduckdb.dylib"

# 显示调试信息
echo "Debug info:"
echo "DYLD_LIBRARY_PATH=${DYLD_LIBRARY_PATH}"
echo "DUCKDB_LIBRARY_PATH=${DUCKDB_LIBRARY_PATH}"
echo "DuckDB library details:"
ls -lh "${DUCKDB_LIBRARY_PATH}"
file "${DUCKDB_LIBRARY_PATH}"
nm "${DUCKDB_LIBRARY_PATH}" | grep duckdb_open
echo "Test binary details:"
file "${BUILD_DIR}/test/black/poly/test_poly_duckdb"

"${BUILD_DIR}/test/black/poly/test_poly_duckdb"

echo -e "${GREEN}Running MemKV tests...${NC}"
"${BUILD_DIR}/test/black/poly/test_poly_memkv"

exit $? 

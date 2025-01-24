#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 记录开始时间
START_TIME=$(date +%s.%N)

echo "Building infra library first..."
"$(dirname "$0")/build_infra.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 设置通用的包含路径
INCLUDE_FLAGS="-I${SRC_DIR} -I${TEST_DIR} -I${TEST_DIR}/white -I${PPDB_DIR} -I${PPDB_DIR}/src -I${PPDB_DIR}/src/internal"

echo "Building test framework..."
mkdir -p "${BUILD_DIR}/test/white/framework"
"${CC}" ${CFLAGS} ${INCLUDE_FLAGS} \
    "${PPDB_DIR}/test/white/framework/test_framework.c" \
    -c -o "${BUILD_DIR}/test/white/framework/test_framework.o"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Building mock framework..."
"${CC}" ${CFLAGS} ${INCLUDE_FLAGS} \
    "${PPDB_DIR}/test/white/framework/mock_framework.c" \
    -c -o "${BUILD_DIR}/test/white/framework/mock_framework.o"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Building memory mock..."
mkdir -p "${BUILD_DIR}/test/white/infra"
"${CC}" ${CFLAGS} ${INCLUDE_FLAGS} \
    "${PPDB_DIR}/test/white/infra/mock_memory.c" \
    -c -o "${BUILD_DIR}/test/white/infra/mock_memory.o"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Building test cases..."
"${CC}" ${CFLAGS} ${INCLUDE_FLAGS} \
    "${PPDB_DIR}/test/white/infra/test_memory_mock.c" \
    -c -o "${BUILD_DIR}/test/white/infra/test_memory_mock.o"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Linking test executables..."
"${CC}" \
    "${BUILD_DIR}/test/white/framework/test_framework.o" \
    "${BUILD_DIR}/test/white/framework/mock_framework.o" \
    "${BUILD_DIR}/test/white/infra/mock_memory.o" \
    "${BUILD_DIR}/test/white/infra/test_memory_mock.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    ${LDFLAGS} ${LIBS} \
    -o "${BUILD_DIR}/test/white/infra/test_memory_mock.exe"
if [ $? -ne 0 ]; then
    exit 1
fi

#"${OBJCOPY}" -S -O binary \
#    "${BUILD_DIR}/test/white/infra/test_memory_mock.exe.dbg" \
#    "${BUILD_DIR}/test/white/infra/test_memory_mock.exe"
#if [ $? -ne 0 ]; then
#    exit 1
#fi

# 如果没有传入 norun 参数，则运行测试
if [ "$1" != "norun" ]; then
    "${BUILD_DIR}/test/white/infra/test_memory_mock.exe"
fi

# 计算并显示构建时间
END_TIME=$(date +%s.%N)
DIFF=$(echo "$END_TIME - $START_TIME" | bc)
printf "Build time: %.3f seconds\n" $DIFF

echo "Build complete."
exit 0 

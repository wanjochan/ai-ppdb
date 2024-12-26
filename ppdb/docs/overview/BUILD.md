# PPDB 构建指南

## 1. 构建环境

### 1.1 环境要求
- 操作系统：
  - Windows 10/11
  - Ubuntu 20.04或以上
  - macOS 10.15或以上
- 工具链：
  - cosmocc (已包含在项目中)
  - CMake 3.15或以上
  - Git
  - Python 3.8或以上 (可选，用于测试)

### 1.2 目录结构
```
ppdb/
├── src/
│   ├── common/      # 通用组件
│   ├── kvstore/     # 存储引擎
│   ├── wal/         # WAL实现
│   └── network/     # 网络组件
├── include/         # 头文件
├── test/           # 测试文件
├── scripts/        # 构建脚本
└── third_party/    # 第三方依赖
```

## 2. 构建步骤

### 2.1 Windows构建
```batch
# 1. 配置环境变量
set PPDB_ROOT=%CD%
set PATH=%PPDB_ROOT%\tools\cross9\bin;%PATH%

# 2. 创建构建目录
mkdir build
cd build

# 3. 运行CMake
cmake -G "Ninja" ..

# 4. 构建
ninja

# 5. 运行测试
ctest --output-on-failure
```

### 2.2 Linux/macOS构建
```bash
# 1. 配置环境变量
export PPDB_ROOT=$(pwd)
export PATH=$PPDB_ROOT/tools/cross9/bin:$PATH

# 2. 创建构建目录
mkdir build && cd build

# 3. 运行CMake
cmake ..

# 4. 构建
make -j$(nproc)

# 5. 运行测试
ctest --output-on-failure
```

## 3. 构建选项

### 3.1 CMake选项
```cmake
# 启用调试模式
-DCMAKE_BUILD_TYPE=Debug

# 启用测试
-DPPDB_BUILD_TESTS=ON

# 启用性能分析
-DPPDB_ENABLE_PROFILING=ON

# 启用内存检查
-DPPDB_ENABLE_SANITIZER=ON
```

### 3.2 编译选项
```cmake
set(PPDB_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Werror
    -fPIC
    -std=c11
)
```

## 4. 常见问题

### 4.1 构建错误
1. CMake配置错误
   ```
   解决方案：确保CMake版本正确，且PATH中包含正确的工具链
   ```

2. 编译错误
   ```
   解决方案：检查依赖是否完整，确保代码符合C11标准
   ```

3. 链接错误
   ```
   解决方案：检查库文件是否正确生成，路径是否正确
   ```

### 4.2 性能优化
1. 启用优化
   ```cmake
   -DCMAKE_BUILD_TYPE=Release
   ```

2. 使用LTO
   ```cmake
   -DPPDB_ENABLE_LTO=ON
   ```

## 5. 发布构建

### 5.1 版本控制
```bash
# 设置版本号
git tag -a v1.0.0 -m "Release version 1.0.0"

# 推送标签
git push origin v1.0.0
```

### 5.2 发布检查清单
1. 更新版本号
2. 运行完整测试套件
3. 更新文档
4. 创建release notes
5. 打包发布文件

### 5.3 发布包内容
```
ppdb-1.0.0/
├── bin/           # 可执行文件
├── lib/           # 库文件
├── include/       # 头文件
├── docs/          # 文档
└── README.md      # 说明文件
```

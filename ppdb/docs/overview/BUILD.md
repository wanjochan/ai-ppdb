# PPDB 构建指南

> 本文档详细说明了 PPDB 的构建环境、步骤和选项。它与 `DEVELOPMENT.md` 互补，前者关注构建过程，后者关注开发规范。相关文档：
> - 开发规范见 `overview/DEVELOPMENT.md`
> - 系统设计见 `overview/DESIGN.md`
> - 代码改进计划见 `overview/REFACTOR.md`

## 1. 初始化环境

### 1.1 目录准备
```bash
# 在项目根目录创建 repos 目录（此目录不会提交到远程）
mkdir -p repos
cd repos
```

### 1.2 克隆依赖仓库
```bash
# 克隆 cosmopolitan 仓库（用于跨平台支持）
git clone https://github.com/jart/cosmopolitan.git
cd cosmopolitan
git checkout v3.2.1  # 使用稳定版本
cd ..

# 克隆其他参考项目
git clone https://github.com/google/leveldb.git
cd leveldb
git checkout v1.23
cd ..
```

### 1.3 下载工具链
```bash
# Windows 平台
## 下载 cosmocc
curl -L https://cosmo.zip/pub/cosmocc/cosmocc.zip -o cosmocc.zip
unzip cosmocc.zip -d cosmocc
mv cosmocc ../tools/

## 下载 cross9
curl -L https://github.com/jart/cosmopolitan/releases/download/3.2.1/cross9.zip -o cross9.zip
unzip cross9.zip -d cross9
mv cross9 ../tools/

## 下载 mingw64（可选，用于本地开发）
curl -L https://github.com/niXman/mingw-builds-binaries/releases/download/13.2.0-rt_v11-rev0/x86_64-13.2.0-release-win32-seh-msvcrt-rt_v11-rev0.7z -o mingw64.7z
7z x mingw64.7z -o../tools/

# Linux/macOS 平台
## 下载 cosmocc
curl -L https://cosmo.zip/pub/cosmocc/cosmocc.zip -o cosmocc.zip
unzip cosmocc.zip -d cosmocc
mv cosmocc ../tools/

## 下载 cross9
curl -L https://github.com/jart/cosmopolitan/releases/download/3.2.1/cross9.tar.gz -o cross9.tar.gz
tar xf cross9.tar.gz
mv cross9 ../tools/
```

### 1.4 验证环境
```bash
# 检查工具链
../tools/cosmocc/bin/cosmocc --version
../tools/cross9/bin/cosmocc --version

# 检查目录结构
tree -L 2 repos/
tree -L 2 tools/

# 验证编译器可用性
echo 'int main() { return 0; }' > test.c
../tools/cosmocc/bin/cosmocc test.c -o test
./test
rm test.c test
```

### 1.5 注意事项
- `repos/` 目录已添加到 `.gitignore`，其内容不会提交到远程
- 所有工具链和依赖库都应放在 `tools/` 或 `repos/` 目录下
- 确保使用指定版本的依赖，避免兼容性问题
- Windows 用户可能需要安装 7-Zip 来解压工具链

## 2. 构建环境

### 2.1 环境要求
- 操作系统：
  - Windows 10/11
  - Ubuntu 20.04或以上
  - macOS 10.15或以上
- 工具链：
  - cosmocc (已包含在项目中)
  - CMake 3.15或以上
  - Git
  - Python 3.8或以上 (可选，用于测试)

### 2.2 目录结构
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

## 3. 构建步骤

### 3.1 Windows构建
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

### 3.2 Linux/macOS构建
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

## 4. 构建选项

### 4.1 CMake选项
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

### 4.2 编译选项
```cmake
set(PPDB_COMPILE_OPTIONS
    -Wall
    -Wextra
    -Werror
    -fPIC
    -std=c11
)
```

## 5. 常见问题

### 5.1 构建错误
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

### 5.2 性能优化
1. 启用优化
   ```cmake
   -DCMAKE_BUILD_TYPE=Release
   ```

2. 使用LTO
   ```cmake
   -DPPDB_ENABLE_LTO=ON
   ```

## 6. 发布构建

### 6.1 版本控制
```bash
# 设置版本号
git tag -a v1.0.0 -m "Release version 1.0.0"

# 推送标签
git push origin v1.0.0
```

### 6.2 发布检查清单
1. 更新版本号
2. 运行完整测试套件
3. 更新文档
4. 创建release notes
5. 打包发布文件

### 6.3 发布包内容
```
ppdb-1.0.0/
├── bin/           # 可执行文件
├── lib/           # 库文件
├── include/       # 头文件
├── docs/          # 文档
└── README.md      # 说明文件
```

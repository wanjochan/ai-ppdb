# PPDB 构建指南

> 本文档详细说明了 PPDB 的构建环境、步骤和选项。它与 `DEVELOPMENT.md` 互补，前者关注构建过程，后者关注开发规范。相关文档：
> - 开发规范见 `overview/DEVELOPMENT.md`
> - 系统设计见 `overview/DESIGN.md`
> - 代码改进计划见 `overview/REFACTOR.md`

## 1. 初始化环境

PPDB 提供了自动化的环境初始化脚本，它们会：
- 创建必要的目录结构
- 下载并安装所需的工具链
- 克隆参考代码仓库
- 验证环境配置

### 1.1 工具链说明
PPDB 在不同平台使用不同的工具链：
- Windows 平台：
  - 使用 cross9 工具链（从 justine.lol 下载）
  - 用于在 Windows 上进行交叉编译
- Linux/macOS 平台：
  - 使用 cosmocc 工具链（从 cosmo.zip 下载）
  - 直接在本地编译

这种设计使得我们能在不同平台上获得最佳的开发体验。

### 1.2 Windows 平台
```batch
# 直接运行
scripts\setup.bat

# 使用代理
scripts\setup.bat http://proxy.example.com:8080
# 或者设置环境变量
set HTTP_PROXY=http://proxy.example.com:8080
scripts\setup.bat
```

### 1.3 Linux/macOS 平台
```bash
# 直接运行
chmod +x scripts/setup.sh
./scripts/setup.sh

# 使用代理
./scripts/setup.sh http://proxy.example.com:8080
# 或者设置环境变量
export HTTP_PROXY=http://proxy.example.com:8080
./scripts/setup.sh
```

### 1.4 目录结构说明
初始化后的目录结构如下：
```
ppdb/
├── repos/              # 第三方代码和工具链
│   ├── leveldb/       # LevelDB 参考实现
│   ├── cosmopolitan/  # 运行时文件
│   └── cross9/        # Windows平台的交叉编译工具链
└── tools/             # 编译工具
    └── cosmocc/       # Linux/macOS平台的编译器和基础工具
```

### 1.5 注意事项
- Windows 用户需要安装 PowerShell 7+ 和 Git
- Linux/macOS 用户需要安装 curl、unzip 和 Git
- 所有路径配置都相对于项目根目录
- 初始化完成后请运行 build 命令验证环境
- 如果需要使用代理，可以通过命令行参数或环境变量设置
- 生成的可执行文件后缀会根据操作系统不同而不同：
  - Windows: `.exe`
  - macOS: `.osx`
  - Linux: `.lnx`
- Windows 批处理脚本（.bat）的重要说明：
  * 脚本开头的 `chcp 65001` 命令用于设置控制台代码页为 UTF-8
  * 这行代码对于正确显示中文输出至关重要
  * 在修改脚本时必须保留这个设置
  * 格式：`chcp 65001 > nul`（重定向到 nul 避免显示切换提示）

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

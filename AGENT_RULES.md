# Agent Rules

**重要提示：所有 AI助理、智能体（比如 Cursor，Windsurf, Devin, Cline等）必须经常阅读并遵循本文档作为基本准则**

1. **语言使用规范**
   - 对话可用中文，保持简洁（concise）和精准（precise）
   - 项目文档使用中英文皆可
   - 项目源代码特别是注释必须使用英文

2. **开发规范**
   - 新建文件前，先检查是否有类似的文件以免重复新建，实在要新建需要先讨论确认
   - 新建函数或结构体或变量前，先检查是否有类似可重用的内容
   - 修改文件时要细心，避免误删其他内容
   - 遇到问题时应当先重读本文档和项目文档后制定方案他计划，要善用平行处理（parallel_apply）和批处理

3. **必要时上网寻求解决方案**
   - 遇到解决不了的问题，可使用curl工具（使用代理http://127.0.0.1:8888）寻找解决方案

## ppdb/

- 这是由智能助理们协助编写的分布式数据库项目
- 项目使用 cosmopolitan 底层，.h/.c代码的引用部分不需要用libc头文件
- 项目文档位于 ppdb/docs/，请务必市场翻阅复习
- 构建工具（cosmocc,cross9）应已经在项目仓根目录的 repos/ 下，找不到时要跟用户确认不要自己去建立环境或下载构建工具等
- 新功能必须遵循现有的分层架构
- 主要的引用头如下，如果不是这样应该观察构建脚本是不是没有配置好引用目录
```
#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"
#include "internal/storage.h"
#include "internal/peer.h"
#include "ppdb.h"
```

## cosmo/

- 【实验项目】使用 cosmopolitan 开发的小工具项目

### repos/

- 用于存放其他库或下载文件
- 本目录不是项目，不要提交和推送

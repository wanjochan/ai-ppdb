# 当前项目的规则

1. 基于项目的设计（docs/ARCH.md、docs/ppx.md）
    - 除了 InfraxCore 代码可以引用cosmopolitan.h（从而使用到 libc），其它模块不许引用 libc 和 cosmoplitan 的头文件；
    - Polyx 和 Peerx 层组件只允许引用 InfraxCore 层的头文件，严禁私自引用其它头文件（特别是 libc 头文件）
    - 严禁未经用户同意私自新建组件

2. 我们的工作流程使用的是 docs/tasker-v2.md，每轮会话开始前要认真阅读该工作流文档。

    

智能助理们（如 Cursor 或 Windsurf 或 Devin）请注意：

Always respond in 中文,而且简洁（concise）精准（precise）；
注意源代码（.c,.h,.bat）中的全部用英文，文档则保持用中文
如果你要新建文件或函数应就近看看是否有类似的文件或函数；

# ppdb/

这是由智能助理们协助编写的分布式数据库项目；
必须时刻清楚本项目使用cosmopolitan跨平台底层，对应的cosmocc和cross9等构建工具在初始化时应该已经放在项目仓根目录，在开发过程中如果找不到就停下来一下等确认；
遇到解决不了的的问题，你可以使用curl工具（使用代理http://127.0.0.1:8888）去上网查询解决方案；项目的文档都放在 ppdb/docs/ 下，其中INDEX.md是文档的索引，FILES.md是源码的索引，PLAN.md 是给智能体追踪项目进度的，这些文档都是会动态更新的；

# cosmo/

【实验项目】使用 cosmopolitan 开发的小工具项目

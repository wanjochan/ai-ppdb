# Agent Rules

**重要提示：所有 AI助理、智能体（比如 Cursor，Windsurf, Devin, Cline等）必须经常阅读并遵循本文档作为基本准则**

1. **语言使用规范**
   - 对话必须用中文，保持简洁（concise）和精准（precise）
   - 项目文档使用中英文皆可
   - 项目源代码包括头文件中，特别是注释必须用英文

2. **开发规范**
   - 必须经常阅读项目的设计文档确保不偏离项目、计划、任务的主题
   - 新建文件前，先检查是否有类似的文件以免重复新建，实在要新建需要先讨论确认
   - 新建函数或结构体或变量前，先检查是否有类似可重用的内容
   - 修改文件时要细心，避免误删其他内容
   - 遇到问题时应当先重读本文档和项目文档后制定方案他计划，要善用平行处理（parallel_apply）和批处理

3. **必要时上网寻求解决方案**
   - 遇到解决不了的问题，可使用curl工具（使用代理http://127.0.0.1:8888）寻找解决方案
```
"C:\Program Files\Git\mingw64\bin\curl.exe" --help
Usage: curl [options...] <url>
 -d, --data <data>           HTTP POST data
 -f, --fail                  Fail fast with no output on HTTP errors
 -h, --help <subject>        Get help for commands
 -o, --output <file>         Write to file instead of stdout
 -O, --remote-name           Write output to file named as remote file
 -i, --show-headers          Show response headers in output
 -s, --silent                Silent mode
 -T, --upload-file <file>    Transfer local FILE to destination
 -u, --user <user:password>  Server user and password
 -A, --user-agent <name>     Send User-Agent <name> to server
 -v, --verbose               Make the operation more talkative
 -V, --version               Show version number and quit

This is not the full help; this menu is split into categories.
Use "--help category" to get an overview of all categories, which are:
auth, connection, curl, deprecated, dns, file, ftp, global, http, imap, ldap, output, pop3, post, proxy, scp, sftp,
smtp, ssh, telnet, tftp, timeout, tls, upload, verbose.
Use "--help all" to list all options
Use "--help [option]" to view documentation for a given option

```

## ppdb/

- 由智能助理们协助编写的分布式数据库项目
- 必须阅读 ppdb/docs/DESIGN.md 文件，里面有 ppdb 项目架构的说明

## cosmo/

- 【实验项目】使用 cosmopolitan 开发的小工具项目

### repos/

- 用于存放其他库或下载文件
- 本目录不是项目，不要提交和推送

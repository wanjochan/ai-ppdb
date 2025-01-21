# ELF (Executable and Linkable Format) 文件格式

ELF 是类 Unix 系统中使用的标准二进制文件格式。

## 文件结构

### ELF Header
- 文件标识信息
- 目标机器类型
- 入口点地址
- 程序头表和节头表的位置

### Program Headers
- 描述段信息
- 包含加载地址、大小、权限等
- 主要类型：LOAD、DYNAMIC、INTERP 等

### Sections
- .text: 代码段
- .data: 已初始化数据
- .bss: 未初始化数据
- .symtab: 符号表
- .strtab: 字符串表
- .rel/.rela: 重定位信息

## 重定位
- R_X86_64_RELATIVE
- R_X86_64_GLOB_DAT
- R_X86_64_JUMP_SLOT
- R_X86_64_64

## 动态链接
- 动态符号表(.dynsym)
- 动态字符串表(.dynstr)
- 动态重定位(.rel.dyn/.rela.dyn)
- PLT/GOT 机制

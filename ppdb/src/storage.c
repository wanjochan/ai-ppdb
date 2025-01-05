/*
 * storage.c - PPDB存储层实现
 *
 * 本文件是PPDB存储层的主入口，负责组织和初始化所有存储模块。
 */

#include <cosmopolitan.h>
#include <ppdb/base.h>
#include <internal/storage.h>

// Include implementation files
#include "storage/storage_table.inc.c"
#include "storage/storage_ops.inc.c"
#include "storage/storage_index.inc.c"

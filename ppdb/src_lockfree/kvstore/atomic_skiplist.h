#ifndef PPDB_ATOMIC_SKIPLIST_H
#define PPDB_ATOMIC_SKIPLIST_H

#include <cosmopolitan.h>

// 标记位，用于并发操作
#define PPDB_MARK_MASK    0x1
#define PPDB_FLAG_MASK    0x2
#define PPDB_NODEREF_MASK (~0x3)

// 跳表节点
typedef struct atomic_skipnode_t {
    uint8_t* key;
    size_t key_len;
    uint8_t* value;
    size_t value_len;
    atomic_ulong version;      // 版本号，用于 ABA 问题
    _Atomic(struct atomic_skipnode_t*) forward[];  // 原子指针数组
} atomic_skipnode_t;

// 跳表结构
typedef struct atomic_skiplist_t {
    int max_level;            // 最大层数
    atomic_int level;         // 当前最大层数
    atomic_size_t size;       // 节点数量
    atomic_skipnode_t* head;  // 头节点
} atomic_skiplist_t;

// 迭代器结构
typedef struct atomic_skiplist_iter_t {
    atomic_skiplist_t* list;
    atomic_skipnode_t* current;
    atomic_ulong version;     // 用于一致性检查
} atomic_skiplist_iter_t;

// 创建跳表
atomic_skiplist_t* atomic_skiplist_create(int max_level);

// 销毁跳表
void atomic_skiplist_destroy(atomic_skiplist_t* list);

// 插入/更新键值对
// 返回值：0表示成功，非0表示失败
int atomic_skiplist_put(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       const uint8_t* value, size_t value_len);

// 获取键对应的值
// 返回值：0表示成功，1表示未找到，其他值表示错误
int atomic_skiplist_get(atomic_skiplist_t* list,
                       const uint8_t* key, size_t key_len,
                       uint8_t* value, size_t* value_len);

// 删除键值对
// 返回值：0表示成功，1表示未找到，其他值表示错误
int atomic_skiplist_delete(atomic_skiplist_t* list,
                          const uint8_t* key, size_t key_len);

// 获取跳表大小
size_t atomic_skiplist_size(atomic_skiplist_t* list);

// 创建迭代器
atomic_skiplist_iter_t* atomic_skiplist_iter_create(atomic_skiplist_t* list);

// 销毁迭代器
void atomic_skiplist_iter_destroy(atomic_skiplist_iter_t* iter);

// 迭代器获取下一个元素
bool atomic_skiplist_iter_next(atomic_skiplist_iter_t* iter,
                             uint8_t** key, size_t* key_len,
                             uint8_t** value, size_t* value_len);

#endif // PPDB_ATOMIC_SKIPLIST_H

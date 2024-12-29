#ifndef PPDB_SKIPLIST_MUTEX_H
#define PPDB_SKIPLIST_MUTEX_H

#include <cosmopolitan.h>

// 跳表结构
typedef struct skiplist_t skiplist_t;

// 创建跳表
skiplist_t* skiplist_create(void);

// 销毁跳表
void skiplist_destroy(skiplist_t* list);

// 插入/更新键值对
int skiplist_put(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                const uint8_t* value, size_t value_len);

// 获取键对应的值
int skiplist_get(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                uint8_t* value, size_t* value_len);

// 删除键值对
int skiplist_delete(skiplist_t* list,
                   const uint8_t* key, size_t key_len);

// 获取跳表大小
size_t skiplist_size(skiplist_t* list);

// 迭代器结构
typedef struct skiplist_iterator_t skiplist_iterator_t;

// 创建迭代器
skiplist_iterator_t* skiplist_iterator_create(skiplist_t* list);

// 销毁迭代器
void skiplist_iterator_destroy(skiplist_iterator_t* iter);

// 获取下一个键值对
bool skiplist_iterator_next(skiplist_iterator_t* iter,
                          uint8_t** key, size_t* key_size,
                          uint8_t** value, size_t* value_size);

#endif // PPDB_SKIPLIST_MUTEX_H 
#ifndef PPDB_SKIPLIST_H
#define PPDB_SKIPLIST_H

#include <cosmopolitan.h>

// 跳表结构
typedef struct skiplist_t skiplist_t;

// 创建跳表
skiplist_t* skiplist_create(void);

// 销毁跳表
void skiplist_destroy(skiplist_t* list);

// 插入/更新键值对
// 返回值：0表示成功，非0表示失败
int skiplist_put(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                const uint8_t* value, size_t value_len);

// 获取键对应的值
// 返回值：0表示成功，1表示未找到，其他值表示错误
int skiplist_get(skiplist_t* list,
                const uint8_t* key, size_t key_len,
                uint8_t* value, size_t* value_len);

// 删除键值对
// 返回值：0表示成功，1表示未找到，其他值表示错误
int skiplist_delete(skiplist_t* list,
                   const uint8_t* key, size_t key_len);

// 获取跳表大小
size_t skiplist_size(skiplist_t* list);

#endif // PPDB_SKIPLIST_H 
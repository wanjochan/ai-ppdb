/*
 * base_struct.inc.c - Data Structure Implementation
 *
 * This file contains:
 * 1. Linked list operations
 * 2. Hash table operations
 * 3. Skip list operations
 * 4. Counter operations
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// List Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_list_init(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->cleanup = NULL;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_destroy(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;

    ppdb_base_list_node_t* node = list->head;
    while (node) {
        ppdb_base_list_node_t* next = node->next;
        if (list->cleanup) {
            list->cleanup(node->data);
        }
        ppdb_base_mem_free(node);
        node = next;
    }

    memset(list, 0, sizeof(ppdb_base_list_t));
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_push_front(ppdb_base_list_t* list, void* data) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_list_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
    node->data = data;
    node->next = list->head;
    node->prev = NULL;
    
    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    
    list->head = node;
    list->size++;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_push_back(ppdb_base_list_t* list, void* data) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_list_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
    node->data = data;
    node->next = NULL;
    node->prev = list->tail;
    
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    
    list->tail = node;
    list->size++;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_pop_front(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) return PPDB_ERR_PARAM;
    if (!list->head) return PPDB_ERR_EMPTY;
    
    ppdb_base_list_node_t* node = list->head;
    *out_data = node->data;
    
    list->head = node->next;
    if (!list->head) list->tail = NULL;
    list->size--;
    
    ppdb_base_mem_free(node);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_pop_back(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) return PPDB_ERR_PARAM;
    if (!list->tail) return PPDB_ERR_EMPTY;
    
    ppdb_base_list_node_t* node = list->tail;
    *out_data = node->data;
    
    if (list->head == list->tail) {
        list->head = list->tail = NULL;
    } else {
        ppdb_base_list_node_t* prev = list->head;
        while (prev->next != list->tail) prev = prev->next;
        prev->next = NULL;
        list->tail = prev;
    }
    list->size--;
    
    ppdb_base_mem_free(node);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_front(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) {
        return PPDB_ERR_PARAM;
    }

    if (!list->head) {
        *out_data = NULL;
        return PPDB_ERR_EMPTY;
    }

    *out_data = list->head->data;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_back(ppdb_base_list_t* list, void** out_data) {
    if (!list || !out_data) {
        return PPDB_ERR_PARAM;
    }

    if (!list->tail) {
        *out_data = NULL;
        return PPDB_ERR_EMPTY;
    }

    *out_data = list->tail->data;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_size(ppdb_base_list_t* list, size_t* out_size) {
    if (!list || !out_size) {
        return PPDB_ERR_PARAM;
    }

    *out_size = list->size;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_empty(ppdb_base_list_t* list, bool* out_empty) {
    if (!list || !out_empty) {
        return PPDB_ERR_PARAM;
    }

    *out_empty = (list->size == 0);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_clear(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_list_node_t* node = list->head;
    while (node) {
        ppdb_base_list_node_t* next = node->next;
        if (list->cleanup) list->cleanup(node->data);
        ppdb_base_mem_free(node);
        node = next;
    }
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_list_reverse(ppdb_base_list_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    if (!list->head) return PPDB_OK;

    ppdb_base_list_node_t* curr = list->head;
    ppdb_base_list_node_t* temp = NULL;

    list->tail = list->head;

    while (curr) {
        temp = curr->prev;
        curr->prev = curr->next;
        curr->next = temp;
        curr = curr->prev;
    }

    if (temp) {
        list->head = temp->prev;
    }

    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Hash Table Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_hash_init(ppdb_base_hash_t* hash, size_t initial_size) {
    if (!hash) return PPDB_ERR_PARAM;
    if (initial_size == 0) initial_size = 16;  // Default size
    
    hash->buckets = NULL;
    hash->size = 0;
    hash->capacity = initial_size;
    hash->cleanup = NULL;
    
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_hash_node_t*) * initial_size, (void**)&hash->buckets);
    if (err != PPDB_OK) return err;
    
    memset(hash->buckets, 0, sizeof(ppdb_base_hash_node_t*) * initial_size);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_hash_destroy(ppdb_base_hash_t* hash) {
    if (!hash) return PPDB_ERR_PARAM;
    
    for (size_t i = 0; i < hash->capacity; i++) {
        ppdb_base_hash_node_t* node = hash->buckets[i];
        while (node) {
            ppdb_base_hash_node_t* next = node->next;
            if (hash->cleanup) {
                hash->cleanup(node->value);
            }
            ppdb_base_mem_free(node);
            node = next;
        }
    }
    
    ppdb_base_mem_free(hash->buckets);
    hash->buckets = NULL;
    hash->size = 0;
    hash->capacity = 0;
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Skip List Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_skiplist_init(ppdb_base_skiplist_t* list, size_t max_level) {
    if (!list) return PPDB_ERR_PARAM;
    if (max_level > PPDB_MAX_SKIPLIST_LEVEL) max_level = PPDB_MAX_SKIPLIST_LEVEL;
    
    ppdb_base_skiplist_node_t* head = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t), (void**)&head);
    if (err != PPDB_OK) return err;
    
    head->key = NULL;
    head->value = NULL;
    head->key_size = 0;
    head->value_size = 0;
    head->level = max_level;
    
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t*) * max_level, (void**)&head->forward);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(head);
        return err;
    }
    
    for (int i = 0; i < max_level; i++) {
        head->forward[i] = NULL;
    }
    
    list->head = head;
    list->level = 1;
    list->count = 0;
    list->cleanup = NULL;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_destroy(ppdb_base_skiplist_t* list) {
    if (!list) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* node = list->head;
    while (node) {
        ppdb_base_skiplist_node_t* next = node->forward[0];
        if (list->cleanup && node->value) {
            list->cleanup(node->value);
        }
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        node = next;
    }
    
    list->head = NULL;
    list->level = 0;
    list->count = 0;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_size(ppdb_base_skiplist_t* list, size_t* out_size) {
    if (!list || !out_size) return PPDB_ERR_PARAM;
    
    *out_size = list->count;
    return PPDB_OK;
}

static int random_level(void) {
    int level = 1;
    while ((rand() & 0xFFFF) < (0xFFFF >> 1) && level < PPDB_MAX_SKIPLIST_LEVEL) {
        level++;
    }
    return level;
}

ppdb_error_t ppdb_base_skiplist_insert(ppdb_base_skiplist_t* list, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!list || !key || !value) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->head;
    
    // Find position to insert
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && memcmp(current->forward[i]->key, key, key_size) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    
    // Key exists, update value
    if (current && memcmp(current->key, key, key_size) == 0) {
        if (list->cleanup) {
            list->cleanup(current->value);
        }
        void* new_value;
        ppdb_error_t err = ppdb_base_mem_malloc(value_size, &new_value);
        if (err != PPDB_OK) return err;
        memcpy(new_value, value, value_size);
        current->value = new_value;
        current->value_size = value_size;
        return PPDB_OK;
    }
    
    // Create new node
    int level = random_level();
    if (level > list->level) {
        for (int i = list->level; i < level; i++) {
            update[i] = list->head;
        }
        list->level = level;
    }
    
    ppdb_base_skiplist_node_t* node;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t), (void**)&node);
    if (err != PPDB_OK) return err;
    
    node->level = level;
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_skiplist_node_t*) * level, (void**)&node->forward);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(node);
        return err;
    }
    
    // Copy key and value
    void* new_key;
    err = ppdb_base_mem_malloc(key_size, &new_key);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        return err;
    }
    memcpy(new_key, key, key_size);
    
    void* new_value;
    err = ppdb_base_mem_malloc(value_size, &new_value);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_key);
        ppdb_base_mem_free(node->forward);
        ppdb_base_mem_free(node);
        return err;
    }
    memcpy(new_value, value, value_size);
    
    node->key = new_key;
    node->key_size = key_size;
    node->value = new_value;
    node->value_size = value_size;
    
    // Update forward pointers
    for (int i = 0; i < level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    
    list->count++;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_skiplist_find(ppdb_base_skiplist_t* list, const void* key, size_t key_size, void** value, size_t* value_size) {
    if (!list || !key || !value) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* current = list->head;
    
    // Search from top level
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && memcmp(current->forward[i]->key, key, key_size) < 0) {
            current = current->forward[i];
        }
    }
    
    current = current->forward[0];
    if (current && memcmp(current->key, key, key_size) == 0) {
        *value = current->value;
        if (value_size) *value_size = current->value_size;
        return PPDB_OK;
    }
    
    return PPDB_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_base_skiplist_remove(ppdb_base_skiplist_t* list, const void* key, size_t key_size) {
    if (!list || !key) return PPDB_ERR_PARAM;
    
    ppdb_base_skiplist_node_t* update[PPDB_MAX_SKIPLIST_LEVEL];
    ppdb_base_skiplist_node_t* current = list->head;
    
    // 从最高层开始搜索要删除的节点
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && memcmp(current->forward[i]->key, key, key_size) < 0) {
            current = current->forward[i];
        }
        update[i] = current;
    }
    current = current->forward[0];
    
    // 如果找到目标节点
    if (current && memcmp(current->key, key, key_size) == 0) {
        // 更新各层的前向指针
        for (int i = 0; i < list->level; i++) {
            if (update[i]->forward[i] != current) {
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }
        
        // 清理节点数据
        if (list->cleanup && current->value) {
            list->cleanup(current->value);
        }
        ppdb_base_mem_free(current->key);
        ppdb_base_mem_free(current->value);
        ppdb_base_mem_free(current->forward);
        ppdb_base_mem_free(current);
        
        // 更新节点计数
        list->count--;
        
        // 调整最大层数
        while (list->level > 1 && list->head->forward[list->level - 1] == NULL) {
            list->level--;
        }
        
        return PPDB_OK;
    }
    
    return PPDB_ERR_NOT_FOUND;
}

//-----------------------------------------------------------------------------
// Counter Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name) {
    if (!counter || !name) return PPDB_ERR_PARAM;

    void* counter_ptr;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_counter_t), &counter_ptr);
    if (err != PPDB_OK) {
        return err;
    }
    ppdb_base_counter_t* new_counter = (ppdb_base_counter_t*)counter_ptr;

    atomic_init(&new_counter->value, 0);
    new_counter->name = strdup(name);
    if (!new_counter->name) {
        ppdb_base_mem_free(new_counter);
        return PPDB_ERR_MEMORY;
    }
    new_counter->stats_enabled = false;

    *counter = new_counter;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    free(counter->name);
    ppdb_base_mem_free(counter);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_get(ppdb_base_counter_t* counter, uint64_t* out_value) {
    if (!counter || !out_value) return PPDB_ERR_PARAM;
    *out_value = atomic_load(&counter->value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_store(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_add(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_sub(&counter->value, 1);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_add(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_fetch_sub(&counter->value, value);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired) {
    if (!counter) return PPDB_ERR_PARAM;
    uint64_t exp = expected;
    bool success = atomic_compare_exchange_strong(&counter->value, &exp, desired);
    return success ? PPDB_OK : PPDB_ERR_BUSY;
}

ppdb_error_t ppdb_base_counter_reset(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_ERR_PARAM;
    atomic_store(&counter->value, 0);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Timer Implementation
//-----------------------------------------------------------------------------

// Timer wheel structure
typedef struct ppdb_timer_wheel_s {
    ppdb_base_timer_t* slots[PPDB_TIMER_WHEEL_SIZE];
    uint32_t current;
} ppdb_timer_wheel_t;

// Timer manager structure
typedef struct ppdb_timer_manager_s {
    ppdb_timer_wheel_t wheels[PPDB_TIMER_WHEEL_COUNT];
    ppdb_base_mutex_t* lock;
    uint64_t current_time;
    uint64_t start_time;
    size_t total_timers;
    size_t active_timers;
    size_t expired_timers;
    size_t overdue_timers;
    uint64_t total_drift;
} ppdb_timer_manager_t;

// Global timer manager
static ppdb_timer_manager_t* g_timer_manager = NULL;

// Initialize timer manager
static ppdb_error_t init_timer_manager(void) {
    if (g_timer_manager) return PPDB_OK;
    
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_timer_manager_t), (void**)&g_timer_manager);
    if (err != PPDB_OK) return err;
    
    memset(g_timer_manager, 0, sizeof(ppdb_timer_manager_t));
    
    // Initialize mutex
    err = ppdb_base_mutex_create(&g_timer_manager->lock);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(g_timer_manager);
        g_timer_manager = NULL;
        return err;
    }
    
    // Get current time
    err = ppdb_base_time_get_microseconds(&g_timer_manager->current_time);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(g_timer_manager->lock);
        ppdb_base_mem_free(g_timer_manager);
        g_timer_manager = NULL;
        return err;
    }
    
    g_timer_manager->start_time = g_timer_manager->current_time;
    
    return PPDB_OK;
}

// Calculate timer slot
static void calc_timer_slot(uint64_t expires, uint32_t* wheel, uint32_t* slot) {
    uint64_t diff = expires - g_timer_manager->current_time;
    uint64_t ticks = diff / 1000; // Convert to milliseconds
    
    if (ticks < PPDB_TIMER_WHEEL_SIZE) {
        *wheel = 0;
        *slot = (g_timer_manager->wheels[0].current + ticks) & PPDB_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (PPDB_TIMER_WHEEL_BITS * 2)) {
        *wheel = 1;
        *slot = ((ticks >> PPDB_TIMER_WHEEL_BITS) + g_timer_manager->wheels[1].current) & PPDB_TIMER_WHEEL_MASK;
    } else if (ticks < 1 << (PPDB_TIMER_WHEEL_BITS * 3)) {
        *wheel = 2;
        *slot = ((ticks >> (PPDB_TIMER_WHEEL_BITS * 2)) + g_timer_manager->wheels[2].current) & PPDB_TIMER_WHEEL_MASK;
    } else {
        *wheel = 3;
        *slot = ((ticks >> (PPDB_TIMER_WHEEL_BITS * 3)) + g_timer_manager->wheels[3].current) & PPDB_TIMER_WHEEL_MASK;
    }
}

// Add timer to wheel
static ppdb_error_t add_timer_to_wheel(ppdb_base_timer_t* timer) {
    uint32_t wheel, slot;
    calc_timer_slot(timer->next_timeout, &wheel, &slot);
    
    timer->next = g_timer_manager->wheels[wheel].slots[slot];
    g_timer_manager->wheels[wheel].slots[slot] = timer;
    g_timer_manager->active_timers++;
    
    return PPDB_OK;
}

// Cascade timers from higher wheel to lower wheel
static void cascade_timers(uint32_t wheel) {
    ppdb_base_timer_t* curr = g_timer_manager->wheels[wheel].slots[g_timer_manager->wheels[wheel].current];
    g_timer_manager->wheels[wheel].slots[g_timer_manager->wheels[wheel].current] = NULL;
    
    while (curr) {
        ppdb_base_timer_t* next = curr->next;
        add_timer_to_wheel(curr);
        curr = next;
    }
}

// Timer creation
ppdb_error_t ppdb_base_timer_create(ppdb_base_timer_t** timer, uint64_t interval_ms) {
    if (!timer || interval_ms == 0) return PPDB_ERR_PARAM;
    
    ppdb_error_t err = init_timer_manager();
    if (err != PPDB_OK) return err;
    
    ppdb_base_timer_t* new_timer = NULL;
    err = ppdb_base_mem_malloc(sizeof(ppdb_base_timer_t), (void**)&new_timer);
    if (err != PPDB_OK) return err;
    
    new_timer->interval_ms = interval_ms;
    new_timer->next_timeout = g_timer_manager->current_time + interval_ms * 1000;
    new_timer->callback = NULL;
    new_timer->user_data = NULL;
    new_timer->next = NULL;
    new_timer->repeating = false;
    
    memset(&new_timer->stats, 0, sizeof(ppdb_base_timer_stats_t));
    
    err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_timer);
        return err;
    }
    
    err = add_timer_to_wheel(new_timer);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(g_timer_manager->lock);
        ppdb_base_mem_free(new_timer);
        return err;
    }
    
    g_timer_manager->total_timers++;
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    
    *timer = new_timer;
    return PPDB_OK;
}

// Timer destruction
ppdb_error_t ppdb_base_timer_destroy(ppdb_base_timer_t* timer) {
    if (!timer) return PPDB_ERR_PARAM;
    
    ppdb_error_t err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) return err;
    
    // Remove from wheel
    for (int i = 0; i < PPDB_TIMER_WHEEL_COUNT; i++) {
        ppdb_base_timer_t** curr = &g_timer_manager->wheels[i].slots[g_timer_manager->wheels[i].current];
        while (*curr) {
            if (*curr == timer) {
                *curr = timer->next;
                g_timer_manager->active_timers--;
                break;
            }
            curr = &(*curr)->next;
        }
    }
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    
    ppdb_base_mem_free(timer);
    return PPDB_OK;
}

// Timer update
ppdb_error_t ppdb_base_timer_update(void) {
    if (!g_timer_manager) return PPDB_OK;
    
    ppdb_error_t err = ppdb_base_mutex_lock(g_timer_manager->lock);
    if (err != PPDB_OK) return err;
    
    uint64_t now;
    err = ppdb_base_time_get_microseconds(&now);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(g_timer_manager->lock);
        return err;
    }
    
    uint64_t elapsed = (now - g_timer_manager->current_time) / 1000; // Convert to milliseconds
    g_timer_manager->current_time = now;
    
    while (elapsed--) {
        // Process current slot
        ppdb_base_timer_t* curr = g_timer_manager->wheels[0].slots[g_timer_manager->wheels[0].current];
        g_timer_manager->wheels[0].slots[g_timer_manager->wheels[0].current] = NULL;
        
        while (curr) {
            ppdb_base_timer_t* next = curr->next;
            
            // Update statistics
            curr->stats.total_calls++;
            uint64_t actual_elapsed = (now - curr->next_timeout) / 1000;
            curr->stats.last_elapsed = actual_elapsed;
            curr->stats.total_elapsed += actual_elapsed;
            if (actual_elapsed > curr->stats.max_elapsed) curr->stats.max_elapsed = actual_elapsed;
            if (actual_elapsed < curr->stats.min_elapsed || curr->stats.min_elapsed == 0) {
                curr->stats.min_elapsed = actual_elapsed;
            }
            
            // Calculate drift
            int64_t drift = actual_elapsed - curr->interval_ms;
            curr->stats.drift += (drift > 0) ? drift : -drift;
            g_timer_manager->total_drift += (drift > 0) ? drift : -drift;
            
            // Execute callback
            if (curr->callback) {
                curr->callback(curr, curr->user_data);
            }
            
            if (curr->repeating) {
                // Reset for next interval
                curr->next_timeout = now + curr->interval_ms * 1000;
                add_timer_to_wheel(curr);
            } else {
                g_timer_manager->active_timers--;
                g_timer_manager->expired_timers++;
                ppdb_base_mem_free(curr);
            }
            
            curr = next;
        }
        
        // Move to next slot
        g_timer_manager->wheels[0].current = (g_timer_manager->wheels[0].current + 1) & PPDB_TIMER_WHEEL_MASK;
        
        // Cascade timers if needed
        if (g_timer_manager->wheels[0].current == 0) {
            cascade_timers(1);
            if (g_timer_manager->wheels[1].current == 0) {
                cascade_timers(2);
                if (g_timer_manager->wheels[2].current == 0) {
                    cascade_timers(3);
                }
            }
        }
    }
    
    ppdb_base_mutex_unlock(g_timer_manager->lock);
    return PPDB_OK;
}

// Get timer statistics
ppdb_error_t ppdb_base_timer_get_stats(ppdb_base_timer_t* timer,
                                     uint64_t* total_ticks,
                                     uint64_t* min_elapsed,
                                     uint64_t* max_elapsed,
                                     uint64_t* avg_elapsed,
                                     uint64_t* last_elapsed,
                                     uint64_t* drift) {
    if (!timer) return PPDB_ERR_PARAM;
    
    if (total_ticks) *total_ticks = timer->stats.total_calls;
    if (min_elapsed) *min_elapsed = timer->stats.min_elapsed;
    if (max_elapsed) *max_elapsed = timer->stats.max_elapsed;
    if (avg_elapsed && timer->stats.total_calls > 0) {
        *avg_elapsed = timer->stats.total_elapsed / timer->stats.total_calls;
    }
    if (last_elapsed) *last_elapsed = timer->stats.last_elapsed;
    if (drift) *drift = timer->stats.drift;
    
    return PPDB_OK;
}

// Get timer manager statistics
void ppdb_base_timer_get_manager_stats(uint64_t* total_timers,
                                    uint64_t* active_timers,
                                    uint64_t* expired_timers,
                                    uint64_t* overdue_timers,
                                    uint64_t* total_drift) {
    if (!g_timer_manager) return;
    
    if (total_timers) *total_timers = g_timer_manager->total_timers;
    if (active_timers) *active_timers = g_timer_manager->active_timers;
    if (expired_timers) *expired_timers = g_timer_manager->expired_timers;
    if (overdue_timers) *overdue_timers = g_timer_manager->overdue_timers;
    if (total_drift) *total_drift = g_timer_manager->total_drift;
} 
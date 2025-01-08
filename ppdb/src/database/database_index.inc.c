#include "internal/database.h"
#include "internal/base.h"

typedef struct ppdb_database_index_node {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    struct ppdb_database_index_node* next;
} ppdb_database_index_node_t;

typedef struct ppdb_database_index {
    char name[PPDB_MAX_NAME_LEN];
    ppdb_database_index_type_t type;
    ppdb_database_index_node_t* root;
    size_t size;
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock;
} ppdb_database_index_t;

static int database_index_init(ppdb_database_index_t** index, const char* name, ppdb_database_index_type_t type) {
    if (!index || !name) {
        return PPDB_ERR_PARAM;
    }

    *index = (ppdb_database_index_t*)calloc(1, sizeof(ppdb_database_index_t));
    if (!*index) {
        return PPDB_ERR_NOMEM;
    }

    strncpy((*index)->name, name, PPDB_MAX_NAME_LEN);
    (*index)->name[PPDB_MAX_NAME_LEN - 1] = '\0';
    (*index)->type = type;
    (*index)->root = NULL;
    (*index)->size = 0;

    pthread_mutex_init(&(*index)->mutex, NULL);
    pthread_rwlock_init(&(*index)->rwlock, NULL);

    return PPDB_OK;
}

static void database_index_cleanup(ppdb_database_index_t* index) {
    if (!index) {
        return;
    }

    ppdb_database_index_node_t* current = index->root;
    while (current) {
        ppdb_database_index_node_t* next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&index->mutex);
    pthread_rwlock_destroy(&index->rwlock);
}

int ppdb_database_index_create(ppdb_database_t* db, const char* name, ppdb_database_index_type_t type, ppdb_database_index_t** index) {
    if (!db || !name || !index) {
        return PPDB_ERR_PARAM;
    }

    int ret = database_index_init(index, name, type);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Add index to database
    ret = database_index_manager_add_index(db->index_manager, *index);
    if (ret != PPDB_OK) {
        database_index_cleanup(*index);
        free(*index);
        *index = NULL;
        return ret;
    }

    return PPDB_OK;
}

int ppdb_database_index_drop(ppdb_database_t* db, const char* name) {
    if (!db || !name) {
        return PPDB_ERR_PARAM;
    }

    ppdb_database_index_t* index = NULL;
    int ret = database_index_manager_get_index(db->index_manager, name, &index);
    if (ret != PPDB_OK) {
        return ret;
    }

    // Remove index from database
    ret = database_index_manager_remove_index(db->index_manager, name);
    if (ret != PPDB_OK) {
        return ret;
    }

    database_index_cleanup(index);
    free(index);

    return PPDB_OK;
}

int ppdb_database_index_insert(ppdb_database_index_t* index, const void* key, size_t key_size, const void* value, size_t value_size) {
    if (!index || !key || !value) {
        return PPDB_ERR_PARAM;
    }

    ppdb_database_index_node_t* node = (ppdb_database_index_node_t*)malloc(sizeof(ppdb_database_index_node_t));
    if (!node) {
        return PPDB_ERR_NOMEM;
    }

    node->key = malloc(key_size);
    node->value = malloc(value_size);
    if (!node->key || !node->value) {
        free(node->key);
        free(node->value);
        free(node);
        return PPDB_ERR_NOMEM;
    }

    memcpy(node->key, key, key_size);
    memcpy(node->value, value, value_size);
    node->key_size = key_size;
    node->value_size = value_size;

    pthread_rwlock_wrlock(&index->rwlock);
    node->next = index->root;
    index->root = node;
    index->size++;
    pthread_rwlock_unlock(&index->rwlock);

    return PPDB_OK;
}

int ppdb_database_index_find(ppdb_database_index_t* index, const void* key, size_t key_size, void** value, size_t* value_size) {
    if (!index || !key || !value || !value_size) {
        return PPDB_ERR_PARAM;
    }

    pthread_rwlock_rdlock(&index->rwlock);

    ppdb_database_index_node_t* current = index->root;
    while (current) {
        if (current->key_size == key_size && memcmp(current->key, key, key_size) == 0) {
            *value = malloc(current->value_size);
            if (!*value) {
                pthread_rwlock_unlock(&index->rwlock);
                return PPDB_ERR_NOMEM;
            }
            memcpy(*value, current->value, current->value_size);
            *value_size = current->value_size;
            pthread_rwlock_unlock(&index->rwlock);
            return PPDB_OK;
        }
        current = current->next;
    }

    pthread_rwlock_unlock(&index->rwlock);
    return PPDB_ERR_NOT_FOUND;
}

void ppdb_database_index_destroy(ppdb_database_index_t* index) {
    if (!index) {
        return;
    }

    database_index_cleanup(index);
    free(index);
} 
#include "internal/database.h"
#include "internal/base.h"

typedef struct ppdb_database_table_node {
    ppdb_database_table_t* table;
    struct ppdb_database_table_node* next;
} ppdb_database_table_node_t;

typedef struct ppdb_database_table_list {
    ppdb_database_table_node_t* head;
    size_t size;
    pthread_mutex_t mutex;
} ppdb_database_table_list_t;

static int database_table_list_init(ppdb_database_table_list_t** list) {
    if (!list) {
        return PPDB_ERR_PARAM;
    }

    *list = (ppdb_database_table_list_t*)calloc(1, sizeof(ppdb_database_table_list_t));
    if (!*list) {
        return PPDB_ERR_NOMEM;
    }

    (*list)->head = NULL;
    (*list)->size = 0;
    pthread_mutex_init(&(*list)->mutex, NULL);

    return PPDB_OK;
}

static void database_table_list_cleanup(ppdb_database_table_list_t* list) {
    if (!list) {
        return;
    }

    ppdb_database_table_node_t* current = list->head;
    while (current) {
        ppdb_database_table_node_t* next = current->next;
        ppdb_database_table_destroy(current->table);
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&list->mutex);
}

int ppdb_database_table_list_add(ppdb_database_table_list_t* list, ppdb_database_table_t* table) {
    if (!list || !table) {
        return PPDB_ERR_PARAM;
    }

    ppdb_database_table_node_t* node = (ppdb_database_table_node_t*)malloc(sizeof(ppdb_database_table_node_t));
    if (!node) {
        return PPDB_ERR_NOMEM;
    }

    node->table = table;
    
    pthread_mutex_lock(&list->mutex);
    node->next = list->head;
    list->head = node;
    list->size++;
    pthread_mutex_unlock(&list->mutex);

    return PPDB_OK;
}

int ppdb_database_table_list_remove(ppdb_database_table_list_t* list, const char* name) {
    if (!list || !name) {
        return PPDB_ERR_PARAM;
    }

    pthread_mutex_lock(&list->mutex);

    ppdb_database_table_node_t* current = list->head;
    ppdb_database_table_node_t* prev = NULL;

    while (current) {
        if (strcmp(current->table->name, name) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                list->head = current->next;
            }
            list->size--;
            free(current);
            pthread_mutex_unlock(&list->mutex);
            return PPDB_OK;
        }
        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);
    return PPDB_ERR_NOT_FOUND;
}

int ppdb_database_table_list_find(ppdb_database_table_list_t* list, const char* name, ppdb_database_table_t** table) {
    if (!list || !name || !table) {
        return PPDB_ERR_PARAM;
    }

    pthread_mutex_lock(&list->mutex);

    ppdb_database_table_node_t* current = list->head;
    while (current) {
        if (strcmp(current->table->name, name) == 0) {
            *table = current->table;
            pthread_mutex_unlock(&list->mutex);
            return PPDB_OK;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);
    return PPDB_ERR_NOT_FOUND;
}

void ppdb_database_table_list_destroy(ppdb_database_table_list_t* list) {
    if (!list) {
        return;
    }

    database_table_list_cleanup(list);
    free(list);
} 
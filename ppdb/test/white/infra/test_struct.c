/*
 * test_struct.c - Test Infrastructure Data Structures
 */

#include "cosmopolitan.h"
#include "internal/infra/infra.h"
#include "test_framework.h"
#include "../framework/test_framework.h"

static int test_list(void) {
    infra_list_t* list;
    TEST_ASSERT(infra_list_create(&list) == INFRA_OK);
    TEST_ASSERT(list != NULL);

    // Test empty list
    TEST_ASSERT(infra_list_head(list) == NULL);
    TEST_ASSERT(list->size == 0);

    // Test append
    int value1 = 1, value2 = 2, value3 = 3;
    TEST_ASSERT(infra_list_append(list, &value1) == INFRA_OK);
    TEST_ASSERT(infra_list_append(list, &value2) == INFRA_OK);
    TEST_ASSERT(infra_list_append(list, &value3) == INFRA_OK);
    TEST_ASSERT(list->size == 3);

    // Test traversal
    infra_list_node_t* node = infra_list_head(list);
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)infra_list_node_value(node) == 1);
    
    node = infra_list_node_next(node);
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)infra_list_node_value(node) == 2);
    
    node = infra_list_node_next(node);
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)infra_list_node_value(node) == 3);
    
    node = infra_list_node_next(node);
    TEST_ASSERT(node == NULL);

    // Test remove
    node = infra_list_head(list);
    TEST_ASSERT(infra_list_remove(list, node) == INFRA_OK);
    TEST_ASSERT(list->size == 2);
    
    node = infra_list_head(list);
    TEST_ASSERT(*(int*)infra_list_node_value(node) == 2);

    infra_list_destroy(list);
    return 0;
}

static int test_hash(void) {
    infra_hash_t* hash;
    TEST_ASSERT(infra_hash_create(&hash, 16) == INFRA_OK);
    TEST_ASSERT(hash != NULL);

    // Test empty hash
    TEST_ASSERT(infra_hash_get(hash, "key1") == NULL);
    TEST_ASSERT(hash->size == 0);

    // Test put
    int value1 = 1, value2 = 2, value3 = 3;
    TEST_ASSERT(infra_hash_put(hash, "key1", &value1) == INFRA_OK);
    TEST_ASSERT(infra_hash_put(hash, "key2", &value2) == INFRA_OK);
    TEST_ASSERT(infra_hash_put(hash, "key3", &value3) == INFRA_OK);
    TEST_ASSERT(hash->size == 3);

    // Test get
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key1") == 1);
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key2") == 2);
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key3") == 3);
    TEST_ASSERT(infra_hash_get(hash, "key4") == NULL);

    // Test remove
    TEST_ASSERT(*(int*)infra_hash_remove(hash, "key2") == 2);
    TEST_ASSERT(hash->size == 2);
    TEST_ASSERT(infra_hash_get(hash, "key2") == NULL);

    // Test clear
    infra_hash_clear(hash);
    TEST_ASSERT(hash->size == 0);
    TEST_ASSERT(infra_hash_get(hash, "key1") == NULL);
    TEST_ASSERT(infra_hash_get(hash, "key3") == NULL);

    infra_hash_destroy(hash);
    return 0;
}

static int test_rbtree(void) {
    infra_rbtree_t* tree;
    TEST_ASSERT(infra_rbtree_create(&tree) == INFRA_OK);
    TEST_ASSERT(tree != NULL);

    // Test empty tree
    TEST_ASSERT(infra_rbtree_find(tree, 1) == NULL);
    TEST_ASSERT(tree->size == 0);

    // Test insert
    int value1 = 1, value2 = 2, value3 = 3;
    TEST_ASSERT(infra_rbtree_insert(tree, 2, &value2) == INFRA_OK);
    TEST_ASSERT(infra_rbtree_insert(tree, 1, &value1) == INFRA_OK);
    TEST_ASSERT(infra_rbtree_insert(tree, 3, &value3) == INFRA_OK);
    TEST_ASSERT(tree->size == 3);

    // Test find
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 1) == 1);
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 2) == 2);
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 3) == 3);
    TEST_ASSERT(infra_rbtree_find(tree, 4) == NULL);

    // Test remove
    TEST_ASSERT(*(int*)infra_rbtree_remove(tree, 2) == 2);
    TEST_ASSERT(tree->size == 2);
    TEST_ASSERT(infra_rbtree_find(tree, 2) == NULL);

    // Test clear
    infra_rbtree_clear(tree);
    TEST_ASSERT(tree->size == 0);
    TEST_ASSERT(infra_rbtree_find(tree, 1) == NULL);
    TEST_ASSERT(infra_rbtree_find(tree, 3) == NULL);

    infra_rbtree_destroy(tree);
    return 0;
}

int main(void) {
    TEST_INIT();

    TEST_RUN(test_list);
    TEST_RUN(test_hash);
    TEST_RUN(test_rbtree);

    TEST_CLEANUP();
    return 0;
} 

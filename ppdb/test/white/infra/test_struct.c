/*
 * test_struct.c - Test Infrastructure Data Structures
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"
#include "test/white/framework/test_framework.h"

static void test_list(void) {
    infra_list_t* list = NULL;
    int value1 = 1, value2 = 2, value3 = 3;
    infra_list_node_t* node = NULL;

    TEST_ASSERT(infra_list_create(&list) == INFRA_OK);
    TEST_ASSERT(list != NULL);

    TEST_ASSERT(infra_list_append(list, &value1) == INFRA_OK);
    TEST_ASSERT(infra_list_append(list, &value2) == INFRA_OK);
    TEST_ASSERT(infra_list_append(list, &value3) == INFRA_OK);
    TEST_ASSERT(list->size == 3);

    node = list->head;
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)node->value == 1);

    node = node->next;
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)node->value == 2);

    node = node->next;
    TEST_ASSERT(node != NULL);
    TEST_ASSERT(*(int*)node->value == 3);

    node = node->next;
    TEST_ASSERT(node == NULL);

    node = list->head;
    TEST_ASSERT(infra_list_remove(list, node) == INFRA_OK);
    TEST_ASSERT(list->size == 2);

    node = list->head;
    TEST_ASSERT(*(int*)node->value == 2);

    infra_list_destroy(list);
}

static void test_hash(void) {
    infra_hash_t* hash = NULL;
    int value1 = 1, value2 = 2, value3 = 3;

    TEST_ASSERT(infra_hash_create(&hash, 16) == INFRA_OK);
    TEST_ASSERT(hash != NULL);

    // Test empty hash table
    TEST_ASSERT(infra_hash_get(hash, "key1") == NULL);
    TEST_ASSERT(hash->size == 0);

    // Test inserting elements
    TEST_ASSERT(infra_hash_put(hash, "key1", &value1) == INFRA_OK);
    TEST_ASSERT(infra_hash_put(hash, "key2", &value2) == INFRA_OK);
    TEST_ASSERT(infra_hash_put(hash, "key3", &value3) == INFRA_OK);
    TEST_ASSERT(hash->size == 3);

    // Test retrieving elements
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key1") == 1);
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key2") == 2);
    TEST_ASSERT(*(int*)infra_hash_get(hash, "key3") == 3);
    TEST_ASSERT(infra_hash_get(hash, "key4") == NULL);

    // Test removing elements
    TEST_ASSERT(*(int*)infra_hash_remove(hash, "key2") == 2);
    TEST_ASSERT(hash->size == 2);
    TEST_ASSERT(infra_hash_get(hash, "key2") == NULL);

    // Test clearing hash table
    infra_hash_clear(hash);
    TEST_ASSERT(hash->size == 0);
    TEST_ASSERT(infra_hash_get(hash, "key1") == NULL);
    TEST_ASSERT(infra_hash_get(hash, "key3") == NULL);

    infra_hash_destroy(hash);
}

static void test_rbtree(void) {
    infra_rbtree_t* tree = NULL;
    int value1 = 1, value2 = 2, value3 = 3;

    TEST_ASSERT(infra_rbtree_create(&tree) == INFRA_OK);
    TEST_ASSERT(tree != NULL);

    // Test empty tree
    TEST_ASSERT(infra_rbtree_find(tree, 1) == NULL);
    TEST_ASSERT(tree->size == 0);

    // Test inserting elements
    TEST_ASSERT(infra_rbtree_insert(tree, 2, &value2) == INFRA_OK);
    TEST_ASSERT(infra_rbtree_insert(tree, 1, &value1) == INFRA_OK);
    TEST_ASSERT(infra_rbtree_insert(tree, 3, &value3) == INFRA_OK);
    TEST_ASSERT(tree->size == 3);

    // Test finding elements
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 1) == 1);
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 2) == 2);
    TEST_ASSERT(*(int*)infra_rbtree_find(tree, 3) == 3);
    TEST_ASSERT(infra_rbtree_find(tree, 4) == NULL);

    // Test removing elements
    TEST_ASSERT(*(int*)infra_rbtree_remove(tree, 2) == 2);
    TEST_ASSERT(tree->size == 2);
    TEST_ASSERT(infra_rbtree_find(tree, 2) == NULL);

    // Test clearing tree
    infra_rbtree_clear(tree);
    TEST_ASSERT(tree->size == 0);
    TEST_ASSERT(infra_rbtree_find(tree, 1) == NULL);
    TEST_ASSERT(infra_rbtree_find(tree, 3) == NULL);

    infra_rbtree_destroy(tree);
}

int main(void) {
    TEST_BEGIN();

    RUN_TEST(test_list);
    RUN_TEST(test_hash);
    RUN_TEST(test_rbtree);

    TEST_END();
} 

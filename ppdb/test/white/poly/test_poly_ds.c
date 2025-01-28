#include "unity.h"
#include "internal/poly/poly_ds.h"

static poly_list_t* list;
static poly_hash_t* hash;
static poly_rbtree_t* tree;

void setUp(void) {
    TEST_ASSERT_EQUAL(POLY_OK, poly_list_create(&list));
    TEST_ASSERT_EQUAL(POLY_OK, poly_hash_create(&hash, 16));
    TEST_ASSERT_EQUAL(POLY_OK, poly_rbtree_create(&tree));
}

void tearDown(void) {
    poly_list_destroy(list);
    poly_hash_destroy(hash);
    poly_rbtree_destroy(tree);
}

//-----------------------------------------------------------------------------
// List Tests
//-----------------------------------------------------------------------------

void test_list_empty(void) {
    TEST_ASSERT_NULL(poly_list_head(list));
    TEST_ASSERT_EQUAL(0, list->size);
}

void test_list_append(void) {
    int value1 = 1, value2 = 2;
    
    TEST_ASSERT_EQUAL(POLY_OK, poly_list_append(list, &value1));
    TEST_ASSERT_EQUAL(POLY_OK, poly_list_append(list, &value2));
    TEST_ASSERT_EQUAL(2, list->size);
    
    poly_list_node_t* node = poly_list_head(list);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(&value1, poly_list_node_value(node));
    
    node = poly_list_node_next(node);
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(&value2, poly_list_node_value(node));
    
    TEST_ASSERT_NULL(poly_list_node_next(node));
}

void test_list_remove(void) {
    int value1 = 1, value2 = 2, value3 = 3;
    
    poly_list_append(list, &value1);
    poly_list_append(list, &value2);
    poly_list_append(list, &value3);
    
    poly_list_node_t* node = poly_list_head(list);
    node = poly_list_node_next(node);  // Get to value2
    
    TEST_ASSERT_EQUAL(POLY_OK, poly_list_remove(list, node));
    TEST_ASSERT_EQUAL(2, list->size);
    
    node = poly_list_head(list);
    TEST_ASSERT_EQUAL(&value1, poly_list_node_value(node));
    node = poly_list_node_next(node);
    TEST_ASSERT_EQUAL(&value3, poly_list_node_value(node));
}

//-----------------------------------------------------------------------------
// Hash Table Tests
//-----------------------------------------------------------------------------

void test_hash_empty(void) {
    TEST_ASSERT_NULL(poly_hash_get(hash, "nonexistent"));
    TEST_ASSERT_EQUAL(0, hash->size);
}

void test_hash_put_get(void) {
    int value1 = 1, value2 = 2;
    
    TEST_ASSERT_EQUAL(POLY_OK, poly_hash_put(hash, "key1", &value1));
    TEST_ASSERT_EQUAL(POLY_OK, poly_hash_put(hash, "key2", &value2));
    TEST_ASSERT_EQUAL(2, hash->size);
    
    TEST_ASSERT_EQUAL(&value1, poly_hash_get(hash, "key1"));
    TEST_ASSERT_EQUAL(&value2, poly_hash_get(hash, "key2"));
    TEST_ASSERT_NULL(poly_hash_get(hash, "nonexistent"));
}

void test_hash_remove(void) {
    int value = 1;
    
    poly_hash_put(hash, "key", &value);
    TEST_ASSERT_EQUAL(&value, poly_hash_get(hash, "key"));
    
    poly_hash_remove(hash, "key");
    TEST_ASSERT_NULL(poly_hash_get(hash, "key"));
    TEST_ASSERT_EQUAL(0, hash->size);
}

void test_hash_clear(void) {
    int value1 = 1, value2 = 2;
    
    poly_hash_put(hash, "key1", &value1);
    poly_hash_put(hash, "key2", &value2);
    
    poly_hash_clear(hash);
    TEST_ASSERT_EQUAL(0, hash->size);
    TEST_ASSERT_NULL(poly_hash_get(hash, "key1"));
    TEST_ASSERT_NULL(poly_hash_get(hash, "key2"));
}

//-----------------------------------------------------------------------------
// Red-Black Tree Tests
//-----------------------------------------------------------------------------

void test_rbtree_empty(void) {
    TEST_ASSERT_NULL(poly_rbtree_find(tree, 1));
    TEST_ASSERT_EQUAL(0, tree->size);
}

void test_rbtree_insert_find(void) {
    int value1 = 1, value2 = 2;
    
    TEST_ASSERT_EQUAL(POLY_OK, poly_rbtree_insert(tree, 10, &value1));
    TEST_ASSERT_EQUAL(POLY_OK, poly_rbtree_insert(tree, 20, &value2));
    TEST_ASSERT_EQUAL(2, tree->size);
    
    TEST_ASSERT_EQUAL(&value1, poly_rbtree_find(tree, 10));
    TEST_ASSERT_EQUAL(&value2, poly_rbtree_find(tree, 20));
    TEST_ASSERT_NULL(poly_rbtree_find(tree, 30));
}

void test_rbtree_remove(void) {
    int value1 = 1, value2 = 2;
    
    poly_rbtree_insert(tree, 10, &value1);
    poly_rbtree_insert(tree, 20, &value2);
    
    poly_rbtree_remove(tree, 10);
    TEST_ASSERT_NULL(poly_rbtree_find(tree, 10));
    TEST_ASSERT_EQUAL(&value2, poly_rbtree_find(tree, 20));
    TEST_ASSERT_EQUAL(1, tree->size);
}

void test_rbtree_clear(void) {
    int value1 = 1, value2 = 2;
    
    poly_rbtree_insert(tree, 10, &value1);
    poly_rbtree_insert(tree, 20, &value2);
    
    poly_rbtree_clear(tree);
    TEST_ASSERT_EQUAL(0, tree->size);
    TEST_ASSERT_NULL(poly_rbtree_find(tree, 10));
    TEST_ASSERT_NULL(poly_rbtree_find(tree, 20));
}

int main(void) {
    UNITY_BEGIN();
    
    // List Tests
    RUN_TEST(test_list_empty);
    RUN_TEST(test_list_append);
    RUN_TEST(test_list_remove);
    
    // Hash Table Tests
    RUN_TEST(test_hash_empty);
    RUN_TEST(test_hash_put_get);
    RUN_TEST(test_hash_remove);
    RUN_TEST(test_hash_clear);
    
    // Red-Black Tree Tests
    RUN_TEST(test_rbtree_empty);
    RUN_TEST(test_rbtree_insert_find);
    RUN_TEST(test_rbtree_remove);
    RUN_TEST(test_rbtree_clear);
    
    return UNITY_END();
}

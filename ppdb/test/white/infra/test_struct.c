#include "cosmopolitan.h"
#include "internal/infra/infra.h"

static int test_count = 0;
static int fail_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (!(cond)) { \
        printf("FAIL: %s\n", msg); \
        fail_count++; \
        return -1; \
    } \
} while(0)

static int test_queue(void) {
    printf("Testing queue implementation...\n");
    
    struct infra_queue queue;
    infra_queue_init(&queue);
    
    // Test empty queue
    TEST_ASSERT(infra_queue_empty(&queue), "New queue should be empty");
    TEST_ASSERT(infra_queue_size(&queue) == 0, "New queue size should be 0");
    TEST_ASSERT(infra_queue_pop(&queue) == NULL, "Pop from empty queue should return NULL");
    
    // Test push
    int data1 = 1, data2 = 2, data3 = 3;
    TEST_ASSERT(infra_queue_push(&queue, &data1) == INFRA_OK, "Push 1 failed");
    TEST_ASSERT(infra_queue_push(&queue, &data2) == INFRA_OK, "Push 2 failed");
    TEST_ASSERT(infra_queue_push(&queue, &data3) == INFRA_OK, "Push 3 failed");
    
    TEST_ASSERT(!infra_queue_empty(&queue), "Queue should not be empty after push");
    TEST_ASSERT(infra_queue_size(&queue) == 3, "Queue size should be 3");
    
    // Test pop order (FIFO)
    void* result = infra_queue_pop(&queue);
    TEST_ASSERT(result == &data1, "First pop should return first pushed item");
    result = infra_queue_pop(&queue);
    TEST_ASSERT(result == &data2, "Second pop should return second pushed item");
    result = infra_queue_pop(&queue);
    TEST_ASSERT(result == &data3, "Third pop should return third pushed item");
    
    TEST_ASSERT(infra_queue_empty(&queue), "Queue should be empty after all pops");
    
    printf("Queue test passed\n");
    return 0;
}

// RB-Tree test helper
struct test_rb_node {
    struct infra_rb_node node;
    int key;
};

static int rb_compare(struct infra_rb_node* a, struct infra_rb_node* b) {
    struct test_rb_node* na = (struct test_rb_node*)a;
    struct test_rb_node* nb = (struct test_rb_node*)b;
    return na->key - nb->key;
}

static int test_rbtree(void) {
    printf("Testing red-black tree implementation...\n");
    
    struct infra_rb_tree tree;
    infra_rbtree_init(&tree);
    
    // Test empty tree
    TEST_ASSERT(infra_rbtree_size(&tree) == 0, "New tree should be empty");
    
    struct test_rb_node key_node = {{0}, 42};
    TEST_ASSERT(infra_rbtree_find(&tree, &key_node.node, rb_compare) == NULL,
                "Find in empty tree should return NULL");
    
    // Test insertion
    struct test_rb_node nodes[5];
    int keys[] = {50, 25, 75, 10, 90};
    
    for (int i = 0; i < 5; i++) {
        nodes[i].key = keys[i];
        TEST_ASSERT(infra_rbtree_insert(&tree, &nodes[i].node, rb_compare) == INFRA_OK,
                    "Node insertion failed");
    }
    
    TEST_ASSERT(infra_rbtree_size(&tree) == 5, "Tree size should be 5 after insertions");
    
    // Test find
    key_node.key = 25;
    struct infra_rb_node* found = infra_rbtree_find(&tree, &key_node.node, rb_compare);
    TEST_ASSERT(found != NULL, "Find existing key should succeed");
    TEST_ASSERT(((struct test_rb_node*)found)->key == 25, "Found node should have correct key");
    
    key_node.key = 42;
    found = infra_rbtree_find(&tree, &key_node.node, rb_compare);
    TEST_ASSERT(found == NULL, "Find non-existing key should return NULL");
    
    // Test duplicate insertion
    key_node.key = 50;
    TEST_ASSERT(infra_rbtree_insert(&tree, &key_node.node, rb_compare) == INFRA_ERR_EXISTS,
                "Duplicate insertion should fail");
    
    printf("Red-black tree test passed\n");
    return 0;
}

static int test_main(void) {
    printf("Running data structure tests...\n");
    
    int result = 0;
    result |= test_queue();
    result |= test_rbtree();
    
    printf("Test completed with result: %d\n", result);
    printf("Total tests: %d, Failed: %d\n", test_count, fail_count);
    printf("Test %s\n", result == 0 ? "PASSED" : "FAILED");
    return result;
}

COSMOPOLITAN_C_START_
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return test_main();
}
COSMOPOLITAN_C_END_ 
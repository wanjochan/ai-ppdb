#include "cosmopolitan.h"
#include "internal/infra/infra.h"

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        printf("ASSERT FAILED: %s\n", msg); \
        return 1; \
    }

static int test_queue(void) {
    struct infra_queue queue;
    int data1 = 1, data2 = 2, data3 = 3;
    void* data;

    // Initialize queue
    infra_queue_init(&queue);
    TEST_ASSERT(infra_queue_empty(&queue), "Queue should be empty after init");
    TEST_ASSERT(infra_queue_size(&queue) == 0, "Queue size should be 0 after init");

    // Test push
    TEST_ASSERT(infra_queue_push(&queue, &data1) == PPDB_OK, "Push 1 failed");
    TEST_ASSERT(!infra_queue_empty(&queue), "Queue should not be empty after push");
    TEST_ASSERT(infra_queue_size(&queue) == 1, "Queue size should be 1 after push");

    TEST_ASSERT(infra_queue_push(&queue, &data2) == PPDB_OK, "Push 2 failed");
    TEST_ASSERT(infra_queue_size(&queue) == 2, "Queue size should be 2 after push");

    TEST_ASSERT(infra_queue_push(&queue, &data3) == PPDB_OK, "Push 3 failed");
    TEST_ASSERT(infra_queue_size(&queue) == 3, "Queue size should be 3 after push");

    // Test pop
    data = infra_queue_pop(&queue);
    TEST_ASSERT(data == &data1, "Pop 1 returned wrong data");
    TEST_ASSERT(infra_queue_size(&queue) == 2, "Queue size should be 2 after pop");

    data = infra_queue_pop(&queue);
    TEST_ASSERT(data == &data2, "Pop 2 returned wrong data");
    TEST_ASSERT(infra_queue_size(&queue) == 1, "Queue size should be 1 after pop");

    data = infra_queue_pop(&queue);
    TEST_ASSERT(data == &data3, "Pop 3 returned wrong data");
    TEST_ASSERT(infra_queue_empty(&queue), "Queue should be empty after all pops");
    TEST_ASSERT(infra_queue_size(&queue) == 0, "Queue size should be 0 after all pops");

    // Test pop empty queue
    data = infra_queue_pop(&queue);
    TEST_ASSERT(data == NULL, "Pop empty queue should return NULL");

    return 0;
}

struct test_rb_node {
    struct infra_rb_node node;
    int key;
};

static int rb_compare(struct infra_rb_node* a, struct infra_rb_node* b) {
    struct test_rb_node* node_a = container_of(a, struct test_rb_node, node);
    struct test_rb_node* node_b = container_of(b, struct test_rb_node, node);
    return node_a->key - node_b->key;
}

static int test_rbtree(void) {
    struct infra_rb_tree tree;
    struct test_rb_node nodes[10];
    int i;

    // Initialize tree
    infra_rbtree_init(&tree);
    TEST_ASSERT(infra_rbtree_size(&tree) == 0, "Tree size should be 0 after init");

    // Insert nodes
    for (i = 0; i < 10; i++) {
        nodes[i].key = i;
        TEST_ASSERT(infra_rbtree_insert(&tree, &nodes[i].node, rb_compare) == PPDB_OK,
                   "Insert failed");
        TEST_ASSERT(infra_rbtree_size(&tree) == (size_t)(i + 1),
                   "Tree size incorrect after insert");
    }

    // Test find
    struct test_rb_node key_node;
    struct infra_rb_node* found;

    key_node.key = 5;
    found = infra_rbtree_find(&tree, &key_node.node, rb_compare);
    TEST_ASSERT(found == &nodes[5].node, "Find existing node failed");

    key_node.key = 15;
    found = infra_rbtree_find(&tree, &key_node.node, rb_compare);
    TEST_ASSERT(found == NULL, "Find non-existing node should return NULL");

    // Test duplicate insert
    key_node.key = 5;
    TEST_ASSERT(infra_rbtree_insert(&tree, &key_node.node, rb_compare) == PPDB_ERR_EXISTS,
               "Insert duplicate should fail");

    return 0;
}

int main(void) {
    int result = 0;

    printf("Running queue tests...\n");
    result = test_queue();
    if (result != 0) {
        return result;
    }
    printf("Queue tests passed.\n");

    printf("Running red-black tree tests...\n");
    result = test_rbtree();
    if (result != 0) {
        return result;
    }
    printf("Red-black tree tests passed.\n");

    return 0;
} 
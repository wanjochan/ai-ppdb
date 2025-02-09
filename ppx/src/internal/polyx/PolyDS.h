// //占位，从 infra 中迁移过来未符合规范


// //-----------------------------------------------------------------------------
// // List Operations
// //-----------------------------------------------------------------------------

// typedef struct infra_list_node {
//     struct infra_list_node* next;
//     struct infra_list_node* prev;
//     void* value;
// } infra_list_node_t;

// typedef struct {
//     infra_list_node_t* head;
//     infra_list_node_t* tail;
//     size_t size;
// } infra_list_t;

// infra_error_t infra_list_create(infra_list_t** list);
// void infra_list_destroy(infra_list_t* list);
// infra_error_t infra_list_init(infra_list_t* list);
// void infra_list_cleanup(infra_list_t* list);
// infra_error_t infra_list_append(infra_list_t* list, void* value);
// infra_error_t infra_list_push_back(infra_list_t* list, void* value);
// void* infra_list_pop_front(infra_list_t* list);
// infra_error_t infra_list_remove(infra_list_t* list, infra_list_node_t* node);
// bool infra_list_empty(const infra_list_t* list);
// infra_list_node_t* infra_list_head(infra_list_t* list);
// infra_list_node_t* infra_list_node_next(infra_list_node_t* node);
// void* infra_list_node_value(infra_list_node_t* node);

// //-----------------------------------------------------------------------------
// // Hash Operations
// //-----------------------------------------------------------------------------

// typedef struct infra_hash_node {
//     char* key;
//     void* value;
//     struct infra_hash_node* next;
// } infra_hash_node_t;

// typedef struct {
//     infra_hash_node_t** buckets;
//     size_t size;
//     size_t capacity;
// } infra_hash_t;

// infra_error_t infra_hash_create(infra_hash_t** hash, size_t capacity);
// void infra_hash_destroy(infra_hash_t* hash);
// infra_error_t infra_hash_put(infra_hash_t* hash, const char* key, void* value);
// void* infra_hash_get(infra_hash_t* hash, const char* key);
// void* infra_hash_remove(infra_hash_t* hash, const char* key);
// void infra_hash_clear(infra_hash_t* hash);

// //-----------------------------------------------------------------------------
// // Red-Black Tree Operations
// //-----------------------------------------------------------------------------

// typedef enum {
//     INFRA_RBTREE_RED,
//     INFRA_RBTREE_BLACK
// } infra_rbtree_color_t;

// typedef struct infra_rbtree_node {
//     int key;
//     void* value;
//     infra_rbtree_color_t color;
//     struct infra_rbtree_node* parent;
//     struct infra_rbtree_node* left;
//     struct infra_rbtree_node* right;
// } infra_rbtree_node_t;

// typedef struct {
//     infra_rbtree_node_t* root;
//     infra_rbtree_node_t* nil;  // sentinel node
//     size_t size;
// } infra_rbtree_t;

// infra_error_t infra_rbtree_create(infra_rbtree_t** tree);
// void infra_rbtree_destroy(infra_rbtree_t* tree);
// infra_error_t infra_rbtree_insert(infra_rbtree_t* tree, int key, void* value);
// void* infra_rbtree_find(infra_rbtree_t* tree, int key);
// void* infra_rbtree_remove(infra_rbtree_t* tree, int key);
// void infra_rbtree_clear(infra_rbtree_t* tree);

// //-----------------------------------------------------------------------------
// // Buffer Operations
// //-----------------------------------------------------------------------------

// typedef struct {
//     uint8_t* data;
//     size_t size;
//     size_t capacity;
// } infra_buffer_t;

// infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity);
// void infra_buffer_destroy(infra_buffer_t* buf);
// infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity);
// infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size);
// infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size);
// size_t infra_buffer_readable(const infra_buffer_t* buf);
// size_t infra_buffer_writable(const infra_buffer_t* buf);
// void infra_buffer_reset(infra_buffer_t* buf);

// //-----------------------------------------------------------------------------
// // Ring Buffer Operations
// //-----------------------------------------------------------------------------

// typedef struct {
//     uint8_t* buffer;
//     size_t size;
//     size_t read_pos;
//     size_t write_pos;
//     bool full;
// } infra_ring_buffer_t;

// infra_error_t infra_ring_buffer_init(infra_ring_buffer_t* rb, size_t size);
// void infra_ring_buffer_destroy(infra_ring_buffer_t* rb);
// infra_error_t infra_ring_buffer_write(infra_ring_buffer_t* rb, const void* data, size_t size);
// infra_error_t infra_ring_buffer_read(infra_ring_buffer_t* rb, void* data, size_t size);
// size_t infra_ring_buffer_readable(const infra_ring_buffer_t* rb);
// size_t infra_ring_buffer_writable(const infra_ring_buffer_t* rb);
// void infra_ring_buffer_reset(infra_ring_buffer_t* rb);

// // 时间操作
// uint64_t infra_time_ms(void);

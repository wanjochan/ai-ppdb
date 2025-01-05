#ifndef PPDB_BASE_MEMORY_INC_C
#define PPDB_BASE_MEMORY_INC_C

void* ppdb_aligned_alloc(size_t size) {
    if (size == 0) return NULL;
    
    // Round up size to multiple of alignment
    size_t aligned_size = (size + PPDB_ALIGNMENT - 1) & ~(PPDB_ALIGNMENT - 1);
    
    // Allocate memory with alignment
    void* ptr;
    if (posix_memalign(&ptr, PPDB_ALIGNMENT, aligned_size) != 0) {
        return NULL;
    }
    
    return ptr;
}

void ppdb_aligned_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

ppdb_error_t ppdb_mempool_create(ppdb_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool) return PPDB_ERR_NULL_POINTER;
    if (*pool) return PPDB_ERR_EXISTS;
    if (block_size < sizeof(ppdb_mempool_block_t)) return PPDB_ERR_INVALID_PARAM;
    if (alignment < sizeof(void*)) return PPDB_ERR_INVALID_PARAM;

    ppdb_mempool_t* p = (ppdb_mempool_t*)ppdb_aligned_alloc(sizeof(ppdb_mempool_t));
    if (!p) return PPDB_ERR_OUT_OF_MEMORY;

    p->head = NULL;
    p->block_size = block_size;
    p->alignment = alignment;

    *pool = p;
    return PPDB_OK;
}

void ppdb_mempool_destroy(ppdb_mempool_t* pool) {
    if (!pool) return;

    ppdb_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_mempool_block_t* next = block->next;
        ppdb_aligned_free(block);
        block = next;
    }

    ppdb_aligned_free(pool);
}

void* ppdb_mempool_alloc(ppdb_mempool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;

    // Round up size to alignment
    size_t aligned_size = (size + pool->alignment - 1) & ~(pool->alignment - 1);

    // Find a block with enough space
    ppdb_mempool_block_t* block = pool->head;
    while (block) {
        if (block->size - block->used >= aligned_size) {
            void* ptr = block->data + block->used;
            block->used += aligned_size;
            return ptr;
        }
        block = block->next;
    }

    // Need a new block
    size_t block_size = pool->block_size;
    if (block_size < sizeof(ppdb_mempool_block_t) + aligned_size) {
        block_size = sizeof(ppdb_mempool_block_t) + aligned_size;
    }

    block = (ppdb_mempool_block_t*)ppdb_aligned_alloc(block_size);
    if (!block) return NULL;

    block->next = pool->head;
    block->size = block_size - sizeof(ppdb_mempool_block_t);
    block->used = aligned_size;
    pool->head = block;

    return block->data;
}

#endif // PPDB_BASE_MEMORY_INC_C 
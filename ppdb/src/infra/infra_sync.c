#include "cosmopolitan.h"
#include "internal/infra/infra.h"

/* Linked List Implementation */
void infra_list_init(struct infra_list* list) {
    list->next = list;
    list->prev = list;
}

void infra_list_add(struct infra_list* list, struct infra_list* node) {
    node->next = list->next;
    node->prev = list;
    list->next->prev = node;
    list->next = node;
}

void infra_list_del(struct infra_list* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

int infra_list_empty(struct infra_list* list) {
    return list->next == list;
}

/* Mutex Implementation */
void infra_mutex_init(infra_mutex_t* mutex) {
    mutex->lock.locked = 0;
    infra_list_init(&mutex->waiters);
}

void infra_mutex_destroy(infra_mutex_t* mutex) {
    // Nothing to destroy in our simple implementation
}

static void spin_lock(infra_spinlock_t* lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        // Spin wait
    }
}

static void spin_unlock(infra_spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

void infra_mutex_lock(infra_mutex_t* mutex) {
    spin_lock(&mutex->lock);
}

void infra_mutex_unlock(infra_mutex_t* mutex) {
    spin_unlock(&mutex->lock);
}

int infra_mutex_trylock(infra_mutex_t* mutex) {
    return !__atomic_test_and_set(&mutex->lock.locked, __ATOMIC_ACQUIRE);
}

/* Condition Variable Implementation */
void infra_cond_init(infra_cond_t* cond) {
    infra_list_init(&cond->waiters);
}

void infra_cond_destroy(infra_cond_t* cond) {
    // Nothing to destroy in our simple implementation
}

void infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex) {
    // In a real implementation, this would block the current thread
    // For now, we just unlock and immediately relock the mutex
    infra_mutex_unlock(mutex);
    infra_mutex_lock(mutex);
}

void infra_cond_signal(infra_cond_t* cond) {
    // In a real implementation, this would wake up one waiting thread
}

void infra_cond_broadcast(infra_cond_t* cond) {
    // In a real implementation, this would wake up all waiting threads
}

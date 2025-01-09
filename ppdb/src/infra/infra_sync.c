#include "internal/infra/infra.h"

/* Spinlock Implementation */
void infra_spin_init(infra_spinlock_t* lock) {
    lock->locked = 0;
}

void infra_spin_lock(infra_spinlock_t* lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
            __builtin_ia32_pause();
        }
    }
}

void infra_spin_unlock(infra_spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

int infra_spin_trylock(infra_spinlock_t* lock) {
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

/* Mutex Implementation */
void infra_mutex_init(infra_mutex_t* mutex) {
    infra_spin_init(&mutex->lock);
    infra_list_init(&mutex->waiters);
}

void infra_mutex_destroy(infra_mutex_t* mutex) {
    /* Nothing to do for now */
}

void infra_mutex_lock(infra_mutex_t* mutex) {
    infra_spin_lock(&mutex->lock);
    if (!infra_list_empty(&mutex->waiters)) {
        struct infra_list node;
        infra_list_init(&node);
        infra_list_add(&mutex->waiters, &node);
        infra_spin_unlock(&mutex->lock);
        
        /* Wait for our turn */
        while (1) {
            infra_spin_lock(&mutex->lock);
            if (mutex->waiters.next == &node) {
                infra_list_del(&node);
                return;
            }
            infra_spin_unlock(&mutex->lock);
            __builtin_ia32_pause();
        }
    }
}

void infra_mutex_unlock(infra_mutex_t* mutex) {
    infra_spin_lock(&mutex->lock);
    if (!infra_list_empty(&mutex->waiters)) {
        struct infra_list* next = mutex->waiters.next;
        infra_list_del(next);
    }
    infra_spin_unlock(&mutex->lock);
}

int infra_mutex_trylock(infra_mutex_t* mutex) {
    if (infra_spin_trylock(&mutex->lock)) {
        if (infra_list_empty(&mutex->waiters)) {
            return 1;
        }
        infra_spin_unlock(&mutex->lock);
    }
    return 0;
}

/* Condition Variable Implementation */
void infra_cond_init(infra_cond_t* cond) {
    infra_list_init(&cond->waiters);
}

void infra_cond_destroy(infra_cond_t* cond) {
    /* Nothing to do for now */
}

void infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex) {
    struct infra_list node;
    infra_list_init(&node);
    
    /* Add ourselves to the wait queue */
    infra_spin_lock(&mutex->lock);
    infra_list_add(&cond->waiters, &node);
    infra_spin_unlock(&mutex->lock);
    
    /* Release mutex and wait */
    infra_mutex_unlock(mutex);
    
    /* Wait for signal */
    while (1) {
        infra_spin_lock(&mutex->lock);
        if (node.next == &node) {  /* We've been removed */
            infra_spin_unlock(&mutex->lock);
            break;
        }
        infra_spin_unlock(&mutex->lock);
        __builtin_ia32_pause();
    }
    
    /* Reacquire mutex */
    infra_mutex_lock(mutex);
}

void infra_cond_signal(infra_cond_t* cond) {
    struct infra_list* node;
    
    if (!infra_list_empty(&cond->waiters)) {
        node = cond->waiters.next;
        infra_list_del(node);
        infra_list_init(node);  /* Mark as signaled */
    }
}

void infra_cond_broadcast(infra_cond_t* cond) {
    struct infra_list* node;
    
    while (!infra_list_empty(&cond->waiters)) {
        node = cond->waiters.next;
        infra_list_del(node);
        infra_list_init(node);  /* Mark as signaled */
    }
}

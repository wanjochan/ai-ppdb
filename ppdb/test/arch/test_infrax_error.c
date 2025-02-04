#include "cosmopolitan.h"
#include <assert.h>
#include <pthread.h>
#include "internal/infrax/InfraxError.h"

// 测试基本的错误操作
void test_error_operations(void) {
    InfraxError* error = infrax_error_new();
    assert(error != NULL);
    
    // 测试初始状态
    assert(error->code == 0);
    assert(error->message[0] == '\0');
    
    // 测试设置错误
    error->set(error, -1, "Test error message");
    assert(error->code == -1);
    assert(strcmp(error->get_message(error), "Test error message") == 0);
    
    // 测试清除错误
    error->clear(error);
    assert(error->code == 0);
    assert(error->message[0] == '\0');
    
    error->free(error);
    printf("Basic error operations test passed\n");
}

// 用于线程测试的结构
typedef struct {
    infrax_error_t code;
    const char* message;
} ThreadTestData;

// 线程函数
void* thread_func(void* arg) {
    ThreadTestData* data = (ThreadTestData*)arg;
    
    // 获取线程本地错误实例
    InfraxError* error = get_global_infrax_error();
    assert(error != NULL);
    
    // 设置错误
    error->set(error, data->code, data->message);
    
    // 验证错误状态
    assert(error->code == data->code);
    assert(strcmp(error->get_message(error), data->message) == 0);
    
    return NULL;
}

// 测试线程本地存储
void test_thread_local_storage(void) {
    pthread_t thread1, thread2;
    ThreadTestData data1 = {-1, "Error in thread 1"};
    ThreadTestData data2 = {-2, "Error in thread 2"};
    
    // 创建两个线程
    pthread_create(&thread1, NULL, thread_func, &data1);
    pthread_create(&thread2, NULL, thread_func, &data2);
    
    // 等待线程完成
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    
    // 主线程的错误应该是独立的
    InfraxError* error = get_global_infrax_error();
    assert(error != NULL);
    assert(error->code == 0);  // 应该是初始状态
    
    printf("Thread local storage test passed\n");
}

// 测试全局实例
void test_global_instance(void) {
    InfraxError* error1 = get_global_infrax_error();
    InfraxError* error2 = get_global_infrax_error();
    
    // 同一线程应该返回相同实例
    assert(error1 == error2);
    
    // 测试方法是否正确初始化
    assert(error1->new != NULL);
    assert(error1->free != NULL);
    assert(error1->set != NULL);
    assert(error1->clear != NULL);
    assert(error1->get_message != NULL);
    
    printf("Global instance test passed\n");
}

// 测试错误消息长度限制
void test_message_length_limit(void) {
    InfraxError* error = get_global_infrax_error();
    assert(error != NULL);
    
    // 创建一个超长消息
    char long_message[512];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';
    
    // 设置错误消息
    error->set(error, -1, long_message);
    
    // 验证消息被正确截断
    assert(strlen(error->message) < sizeof(error->message));
    assert(error->message[sizeof(error->message) - 1] == '\0');
    
    printf("Message length limit test passed\n");
}

int main(void) {
    printf("Starting InfraxError tests...\n");
    
    test_error_operations();
    test_thread_local_storage();
    test_global_instance();
    test_message_length_limit();
    
    printf("All InfraxError tests passed!\n");
    return 0;
}

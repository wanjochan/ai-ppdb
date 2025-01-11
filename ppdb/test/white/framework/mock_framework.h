#ifndef MOCK_FRAMEWORK_H
#define MOCK_FRAMEWORK_H

void mock_init(void);
void mock_cleanup(void);
void mock_verify(void);

void mock_function_call(const char* function_name);
void mock_param_value(const char* param_name, uint64_t value);
void mock_param_ptr(const char* param_name, const void* ptr);
void mock_param_str(const char* param_name, const char* str);

void mock_expect_function_call(const char* function_name);
void mock_expect_param_value(const char* param_name, uint64_t value);
void mock_expect_param_ptr(const char* param_name, const void* ptr);
void mock_expect_param_str(const char* param_name, const char* str);

void* mock_expect_return_ptr(const char* function_name, void* ptr);
uint64_t mock_expect_return_value(const char* function_name, uint64_t value);

uint64_t mock_return_value(const char* function_name);
void* mock_return_ptr(const char* function_name);

#endif /* MOCK_FRAMEWORK_H */ 
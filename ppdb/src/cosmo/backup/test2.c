// 全局变量测试
static int global_counter = 0;

// 基本数学运算测试
int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int multiply(int a, int b) {
    return a * b;
}

// 全局状态测试
int increment_counter(void) {
    return ++global_counter;
}

int get_counter(void) {
    return global_counter;
}

// 指针操作测试
void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

// 主入口函数
int module_main(void) {
    // 测试基本运算
    int sum = add(10, 20);        // 应该返回 30
    int diff = subtract(50, 30);  // 应该返回 20
    int prod = multiply(5, 6);    // 应该返回 30

    // 测试全局状态
    int count1 = increment_counter();  // 应该返回 1
    int count2 = increment_counter();  // 应该返回 2

    // 测试指针操作
    int x = 100, y = 200;
    swap(&x, &y);  // x 应该变成 200，y 应该变成 100

    // 返回一个组合值来验证所有操作
    return (sum == 30 && diff == 20 && prod == 30 && 
            count1 == 1 && count2 == 2 && 
            x == 200 && y == 100) ? 42 : -1;
} 
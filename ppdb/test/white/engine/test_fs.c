#include "base/fs/fs.h"
#include "base/test/test_helper.h"
#include "base/common/common.h"

static void test_fs_basic() {
    base_fs_init();
    
    base_fs_create("test.txt");
    base_fs_write("test.txt", "hello", 5);
    
    char buf[10];
    base_fs_read("test.txt", buf, 5);
    
    assert(strcmp(buf, "hello") == 0);
    
    base_fs_delete("test.txt");
    base_fs_cleanup();
}

static void test_fs_advanced() {
    base_fs_init();
    
    base_fs_mkdir("testdir");
    base_fs_create("testdir/file.txt");
    
    base_fs_cleanup(); 
}

void register_fs_tests() {
    TEST_ADD(test_fs_basic);
    TEST_ADD(test_fs_advanced);
}
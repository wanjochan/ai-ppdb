#include "cosmopolitan.h"
//#include <curl/curl.h>
//#include <sys/stat.h>

typedef int (*main_fn)(int argc, char* argv[]);

// 写入回调函数
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

//bool file_exists(const char *filename) {
//    struct stat buffer;
//    return stat(filename, &buffer) == 0;
//}

//bool download_file(const char* url, const char* output_path) {
//    CURL *curl;
//    FILE *fp;
//    CURLcode res;
//    bool success = false;
//
//    curl = curl_easy_init();
//    if (curl) {
//        fp = fopen(output_path, "wb");
//        if (fp) {
//            curl_easy_setopt(curl, CURLOPT_URL, url);
//            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
//            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
//            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
//            
//            res = curl_easy_perform(curl);
//            if (res == CURLE_OK) {
//                success = true;
//            } else {
//                fprintf(stderr, "下载失败: %s\n", curl_easy_strerror(res));
//            }
//            fclose(fp);
//        }
//        curl_easy_cleanup(curl);
//    }
//    return success;
//}

void get_cache_path(const char* url, char* cache_path, size_t size) {
    // 简单的缓存机制：使用URL的最后部分作为文件名
    const char* last_slash = strrchr(url, '/');
    const char* filename = last_slash ? last_slash + 1 : url;
    snprintf(cache_path, size, ".cosmo_cache/%s", filename);
}

void ensure_cache_dir() {
    mkdir(".cosmo_cache", 0755);
}

int load_and_execute(const char* module_name, int argc, char* argv[]) {
    char module_path[512];
    
    if (strstr(module_name, "http://") == module_name || 
        strstr(module_name, "https://") == module_name) {
        
        ensure_cache_dir();
        get_cache_path(module_name, module_path, sizeof(module_path));
        
        //if (!file_exists(module_path)) {
        //    printf("正在从URL下载模块: %s\n", module_name);
        //    if (!download_file(module_name, module_path)) {
        //        fprintf(stderr, "模块下载失败\n");
        //        return 1;
        //    }
        //}
    } else {
        strncpy(module_path, module_name, sizeof(module_path) - 1);
        module_path[sizeof(module_path) - 1] = '\0';
    }
    
    printf("Loaded module: %s\n", module_path);
    printf("Trying to load module: %s\n", module_path);
    void* handle = cosmo_dlopen(module_path, RTLD_NOW);
    if (!handle) {
        printf("Failed to load %s: %s\n", module_path, cosmo_dlerror());
        return 1;
    }

    printf("Module loaded successfully, looking for entry point\n");
    
    // Try to find the entry point
    void *entry = cosmo_dlsym(RTLD_DEFAULT, "module_main");
    printf("Looking for module_main in RTLD_DEFAULT: %p\n", entry);
    if (!entry) {
        entry = cosmo_dlsym(handle, "module_main");
        printf("Looking for module_main in handle: %p\n", entry);
        if (!entry) {
            entry = cosmo_dlsym(handle, "_module_main");
            printf("Looking for _module_main in handle: %p\n", entry);
            if (!entry) {
                entry = cosmo_dlsym(RTLD_DEFAULT, "_module_main");
                printf("Looking for _module_main in RTLD_DEFAULT: %p\n", entry);
                if (!entry) {
                    // Try to list all symbols
                    printf("Available symbols:\n");
                    char cmd[1024];
                    snprintf(cmd, sizeof(cmd), "d:\\dev\\ai-ppdb\\cross9\\bin\\x86_64-pc-linux-gnu-readelf.exe -s d:\\dev\\ai-ppdb\\cosmo\\build\\main.dbg");
                    system(cmd);
                    fprintf(stderr, "Failed to find module_main or _module_main symbol: %s\n", cosmo_dlerror());
                    cosmo_dlclose(handle);
                    return 1;
                }
            }
        }
    }
    printf("Found module entry at %p\n", entry);
    
    // 执行模块
    main_fn entry_point = (main_fn)entry;
    int result = entry_point(argc, argv);
    
    // 清理
    cosmo_dlclose(handle);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <module> [args...]\n", argv[0]);
        return 1;
    }
    
    printf("Loaded module: %s\n", argv[1]);
    return load_and_execute(argv[1], argc - 1, argv + 1);
}

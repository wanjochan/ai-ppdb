#ifndef COSMO_DL_H
#define COSMO_DL_H

/* 动态库加载标志 */
#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_GLOBAL 4
#define RTLD_LOCAL  8

/* 函数声明 */
void* cosmo_dlopen(const char* filename, int flags);
void* cosmo_dlsym(void* handle, const char* symbol);
int cosmo_dlclose(void* handle);
const char* cosmo_dlerror(void);

#endif /* COSMO_DL_H */ 
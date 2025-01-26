# 编译器设置
CC = cc
COSMO_DIR = ../repos/cosmopolitan
COSMO_DLOPEN = $(COSMO_DIR)/libc/dlopen/dlopen.o

# 默认目标
all: main lib/mylib.dylib

# 库的编译规则 (macOS用dylib)
lib/mylib.dylib: lib/mylib.c
	$(CC) -shared -fPIC -o $@ $< \
		-Wl,-exported_symbol,_test_func \
		-Wl,-exported_symbol,_dl_init \
		-Wl,-exported_symbol,_dl_main \
		-Wl,-exported_symbol,_dl_fini

# 主程序编译规则
main: main.c $(COSMO_DLOPEN)
	$(CC) -o $@ $< $(COSMO_DLOPEN) -I$(COSMO_DIR)

.PHONY: clean
clean:
	rm -f main lib/mylib.dylib 
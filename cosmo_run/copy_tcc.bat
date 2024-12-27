@echo off
set SRC_DIR=..\tinycc_src\tinycc-mob
set DST_DIR=tinycc

if not exist %DST_DIR% mkdir %DST_DIR%

copy /y "%SRC_DIR%\libtcc.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\tcc.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\tccgen.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\tccpp.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\tccelf.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\i386-gen.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\x86_64-gen.c" "%DST_DIR%\"
copy /y "%SRC_DIR%\i386-asm.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\x86_64-asm.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\tcctok.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\elf.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\libtcc.h" "%DST_DIR%\"
copy /y "%SRC_DIR%\tcclib.h" "%DST_DIR%\"

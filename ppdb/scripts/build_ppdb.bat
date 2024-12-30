@echo off
setlocal

rem Check paths
echo Checking paths:
set WORKSPACE_ROOT=%~dp0..\..
set COSMO=%WORKSPACE_ROOT%\cosmopolitan
set CROSS9=%WORKSPACE_ROOT%\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

echo WORKSPACE_ROOT: %WORKSPACE_ROOT%
echo COSMO: %COSMO%
echo GCC: %GCC%
echo AR: %AR%

rem Set include paths
set INCLUDE_PATHS=-I%~dp0..\include -I%~dp0..\src -I%~dp0.. -I%COSMO%

rem Set compiler flags
set CFLAGS=-g -O2 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

rem Create build directory
if not exist build mkdir build
cd build

rem Compile common modules
echo Compiling common modules...
echo   Compiling %~dp0..\src\common\error.c...
%GCC% -c -o error.o %~dp0..\src\common\error.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\common\fs.c...
%GCC% -c -o fs.o %~dp0..\src\common\fs.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\common\logger.c...
%GCC% -c -o logger.o %~dp0..\src\common\logger.c %INCLUDE_PATHS% %CFLAGS%

rem Compile KVStore modules
echo Compiling KVStore modules...
echo   Compiling %~dp0..\src\kvstore\kvstore.c...
%GCC% -c -o kvstore.o %~dp0..\src\kvstore\kvstore.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\memtable.c...
%GCC% -c -o memtable.o %~dp0..\src\kvstore\memtable.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\memtable_iterator.c...
%GCC% -c -o memtable_iterator.o %~dp0..\src\kvstore\memtable_iterator.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\metrics.c...
%GCC% -c -o metrics.o %~dp0..\src\kvstore\metrics.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\monitor.c...
%GCC% -c -o monitor.o %~dp0..\src\kvstore\monitor.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\sharded_memtable.c...
%GCC% -c -o sharded_memtable.o %~dp0..\src\kvstore\sharded_memtable.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\skiplist.c...
%GCC% -c -o skiplist.o %~dp0..\src\kvstore\skiplist.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\sync.c...
%GCC% -c -o sync.o %~dp0..\src\kvstore\sync.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\wal.c...
%GCC% -c -o wal.o %~dp0..\src\kvstore\wal.c %INCLUDE_PATHS% %CFLAGS%
echo   Compiling %~dp0..\src\kvstore\kvstore_impl.c...
%GCC% -c -o kvstore_impl.o %~dp0..\src\kvstore\kvstore_impl.c %INCLUDE_PATHS% %CFLAGS%

rem Compile main program
echo Compiling main program...
%GCC% -c -o main.o %~dp0..\src\main.c %INCLUDE_PATHS% %CFLAGS%

rem Create static library
echo Creating static library...
%AR% rcs libppdb.a error.o fs.o logger.o kvstore.o memtable.o memtable_iterator.o metrics.o monitor.o sharded_memtable.o skiplist.o sync.o wal.o kvstore_impl.o

rem Link executable
echo Linking executable...
%GCC% %LDFLAGS% -o ppdb.exe main.o libppdb.a %LIBS%

rem Add APE self-modify support
echo Adding APE self-modify support...
%COSMO%\tool\apelink ppdb.exe

cd ..

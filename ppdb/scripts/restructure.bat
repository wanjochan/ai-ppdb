@echo off
setlocal

rem Create backup of existing code
echo Creating backup...
xcopy /E /I /Y ..\src ..\src.bak

rem Clean up src directory
echo Cleaning up src directory...
rd /S /Q ..\src

rem Create new directory structure
echo Creating new directory structure...
mkdir ..\src\infra
mkdir ..\src\memkv
mkdir ..\src\diskv
mkdir ..\src\ppdb
mkdir ..\src\internal\infra
mkdir ..\src\internal\memkv
mkdir ..\src\internal\diskv
mkdir ..\src\internal\ppdb

rem Create placeholder files
echo Creating placeholder files...
rem Infra layer
echo // TODO: Implementation > ..\src\infra\infra_core.c
echo // TODO: Implementation > ..\src\infra\infra_struct.c
echo // TODO: Implementation > ..\src\infra\infra_sync.c
echo // TODO: Implementation > ..\src\infra\infra_async.c
echo // TODO: Implementation > ..\src\infra\infra_timer.c
echo // TODO: Implementation > ..\src\infra\infra_event.c
echo // TODO: Implementation > ..\src\infra\infra_io.c
echo // TODO: Implementation > ..\src\infra\infra_store.c
echo // TODO: Implementation > ..\src\infra\infra_buffer.c
echo // TODO: Implementation > ..\src\infra\infra_peer.c

rem MemKV layer
echo // TODO: Implementation > ..\src\memkv\memkv.c
echo // TODO: Implementation > ..\src\memkv\memkv_store.c
echo // TODO: Implementation > ..\src\memkv\memkv_peer.c

rem DiskV layer
echo // TODO: Implementation > ..\src\diskv\diskv.c
echo // TODO: Implementation > ..\src\diskv\diskv_store.c
echo // TODO: Implementation > ..\src\diskv\diskv_peer.c

rem PPDB layer
echo // TODO: Implementation > ..\src\ppdb\ppdb.c
echo // TODO: Implementation > ..\src\ppdb\libppdb.c

rem Create header files
echo Creating header files...
echo #pragma once > ..\src\internal\infra\infra.h
echo #pragma once > ..\src\internal\memkv\memkv.h
echo #pragma once > ..\src\internal\diskv\diskv.h
echo #pragma once > ..\src\internal\ppdb\ppdb.h

echo Done! 
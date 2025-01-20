@echo off
setlocal enabledelayedexpansion

call %~dp0\build_env.bat
@rem set SRC_DIR=%ROOT_DIR%\src\internal\infra
@rem echo SRC_DIR=%SRC_DIR%
set BUILD_DIR=%ROOT_DIR%\build\infra

set SRC_FILES=infra_core.c infra_platform.c infra_sync.c infra_error.c infra_ds.c infra_memory.c infra_net.c infra_mux.c infra_mux_epoll.c

rem 记录开始时间
set START_TIME=%time%

rem Build environment setup
call "%~dp0build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directories if they don't exist
if not exist "%BUILD_DIR%\infra" mkdir "%BUILD_DIR%\infra"

echo Checking infra layer...

rem 检查是否需要重新构建
set NEED_REBUILD=0
if not exist "%BUILD_DIR%\infra\libinfra.a" set NEED_REBUILD=1

for %%f in (%SRC_FILES%) do (
    set SRC_FILE=%PPDB_DIR%\src\internal\infra\%%f
    set OBJ_FILE=%BUILD_DIR%\infra\%%~nf.o
    if not exist "!OBJ_FILE!" set NEED_REBUILD=1
    if exist "!SRC_FILE!" if exist "!OBJ_FILE!" (
        for %%i in ("!SRC_FILE!") do set SRC_TIME=%%~ti
        for %%i in ("!OBJ_FILE!") do set OBJ_TIME=%%~ti
        if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_REBUILD=1
    )
)

if !NEED_REBUILD!==1 (
    echo Building infra layer...
    
    rem Build each module
    for %%f in (%SRC_FILES%) do (
        set SRC_FILE=%PPDB_DIR%\src\internal\infra\%%f
        set OBJ_FILE=%BUILD_DIR%\infra\%%~nf.o
        
        set NEED_BUILD=0
        if not exist "!OBJ_FILE!" set NEED_BUILD=1
        if exist "!SRC_FILE!" if exist "!OBJ_FILE!" (
            for %%i in ("!SRC_FILE!") do set SRC_TIME=%%~ti
            for %%i in ("!OBJ_FILE!") do set OBJ_TIME=%%~ti
            if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_BUILD=1
        )
        
        if !NEED_BUILD!==1 (
            echo Building %%~nf...
            "%GCC%" %CFLAGS% -I"%SRC_DIR%" "!SRC_FILE!" -c -o "!OBJ_FILE!" ^
	    -I %PPDB_DIR%\src 
@rem  -I %COSMO% -Wl,-T,%BUILD_DIR%\ape.lds ^
@rem  "%COSMO%\ape.o" "%COSMO%\crt.o" "%COSMO%\cosmopolitan.a"
            if errorlevel 1 exit /b 1
        ) else (
            echo %%~nf is up to date.
        )
    )
    
@rem     rem Create static library
@rem     echo Creating library...
@rem     "%AR%" rcs "%BUILD_DIR%\infra\libinfra.a" "%BUILD_DIR%\infra\infra_core.o" "%BUILD_DIR%\infra\infra_platform.o" "%BUILD_DIR%\infra\infra_sync.o" "%BUILD_DIR%\infra\infra_error.o" "%BUILD_DIR%\infra\infra_ds.o" "%BUILD_DIR%\infra\infra_memory.o" "%BUILD_DIR%\infra\infra_net.o" "%BUILD_DIR%\infra\infra_mux.o" "%BUILD_DIR%\infra\infra_mux_epoll.o"
@rem     if errorlevel 1 exit /b 1
    
    echo Build infra complete.
) else (
    echo Infra layer is up to date.
)

rem 计算耗时
set END_TIME=%time%
set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%START_TIME%") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%END_TIME%") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100

set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 1000%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%

echo Build time: %hours%:%mins%:%secs%.%ms% (HH:MM:SS.MS)

exit /b 0 

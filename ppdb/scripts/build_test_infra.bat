@echo off
setlocal enabledelayedexpansion

rem 记录开始时间
set START_TIME=%time%

rem 检查是否指定了测试模块
set "TEST_MODULE=%~1"

rem 设置测试文件列表
set TEST_FILES=test_memory.c test_log.c test_sync.c test_error.c test_struct.c test_memory_pool.c test_async.c test_net.c test_mux.c

rem 如果没有指定测试模块，显示可用的测试模块
if "%TEST_MODULE%"=="" (
    echo Available test modules:
    for %%f in (%TEST_FILES%) do (
        echo     %%~nf
    )
    exit /b 0
)

call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem 检查infra库是否需要重新构建
set NEED_BUILD_INFRA=0
if not exist "%BUILD_DIR%\infra\libinfra.a" set NEED_BUILD_INFRA=1
for %%f in ("%SRC_DIR%\internal\infra\*.c") do (
    if %%~tf gtr "%BUILD_DIR%\infra\libinfra.a" set NEED_BUILD_INFRA=1
)

if %NEED_BUILD_INFRA%==1 (
    echo Building infra library...
    call "%~dp0\build_infra.bat"
    if errorlevel 1 exit /b 1
) else (
    echo Infra library is up to date.
)

rem 检查测试框架是否需要重新构建
set NEED_BUILD_FRAMEWORK=0
if not exist "%BUILD_DIR%\test\white\framework" mkdir "%BUILD_DIR%\test\white\framework"
if not exist "%BUILD_DIR%\test\white\framework\test_framework.o" (
    set NEED_BUILD_FRAMEWORK=1
) else (
    for %%i in ("%PPDB_DIR%\test\white\framework\test_framework.c") do set SRC_TIME=%%~ti
    for %%i in ("%BUILD_DIR%\test\white\framework\test_framework.o") do set OBJ_TIME=%%~ti
    if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_BUILD_FRAMEWORK=1
)

if !NEED_BUILD_FRAMEWORK!==1 (
    echo Building test framework...
    "%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\test_framework.c" -c -o "%BUILD_DIR%\test\white\framework\test_framework.o"
    if errorlevel 1 exit /b 1
) else (
    echo Test framework is up to date.
)

rem 检查mock框架是否需要重新构建
set NEED_BUILD_MOCK=0
if not exist "%BUILD_DIR%\test\white\framework\mock_framework.o" (
    set NEED_BUILD_MOCK=1
) else (
    for %%i in ("%PPDB_DIR%\test\white\framework\mock_framework.c") do set SRC_TIME=%%~ti
    for %%i in ("%BUILD_DIR%\test\white\framework\mock_framework.o") do set OBJ_TIME=%%~ti
    if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_BUILD_MOCK=1
)

if !NEED_BUILD_MOCK!==1 (
    echo Building mock framework...
    "%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\mock_framework.c" -c -o "%BUILD_DIR%\test\white\framework\mock_framework.o"
    if errorlevel 1 exit /b 1
) else (
    echo Mock framework is up to date.
)

echo Building test cases...
set TEST_FILES=test_memory.c test_log.c test_sync.c test_error.c test_struct.c test_memory_pool.c test_async.c

rem 如果指定了测试模块，则只构建该模块的测试
if not "%TEST_MODULE%"=="" (
    if exist "%PPDB_DIR%\test\white\infra\test_%TEST_MODULE%.c" (
        set TEST_FILES=test_%TEST_MODULE%.c
        echo Building only %TEST_MODULE% module tests...
    ) else (
        echo Error: Test module '%TEST_MODULE%' not found
        echo Available modules:
        for %%f in (%TEST_FILES%) do (
            echo     %%~nf
        )
        exit /b 1
    )
)

for %%f in (%TEST_FILES%) do (
    set NEED_BUILD=0
    if not exist "%BUILD_DIR%\test\white\infra\%%~nf.o" set NEED_BUILD=1
    if "%PPDB_DIR%\test\white\infra\%%f" gtr "%BUILD_DIR%\test\white\infra\%%~nf.o" set NEED_BUILD=1
    
    if !NEED_BUILD!==1 (
        echo Building %%f...
        "%GCC%" %CFLAGS% -I"%PPDB_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\infra\%%f" -c -o "%BUILD_DIR%\test\white\infra\%%~nf.o"
        if errorlevel 1 (
            echo Failed to build %%f
            exit /b 1
        )
    ) else (
        echo %%f is up to date.
    )

    if "%%f"=="test_memory_pool.c" (
        set NEED_BUILD_MAIN=0
        if not exist "%BUILD_DIR%\test\white\infra\test_memory_pool_main.o" set NEED_BUILD_MAIN=1
        if "%PPDB_DIR%\test\white\infra\test_memory_pool_main.c" gtr "%BUILD_DIR%\test\white\infra\test_memory_pool_main.o" set NEED_BUILD_MAIN=1
        
        if !NEED_BUILD_MAIN!==1 (
            echo Building test_memory_pool_main.c...
            "%GCC%" %CFLAGS% -I"%PPDB_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\infra\test_memory_pool_main.c" -c -o "%BUILD_DIR%\test\white\infra\test_memory_pool_main.o"
            if errorlevel 1 (
                echo Failed to build test_memory_pool_main.c
                exit /b 1
            )
        ) else (
            echo test_memory_pool_main.c is up to date.
        )

        set NEED_LINK=0
        if not exist "%BUILD_DIR%\test\white\infra\test_memory_pool.exe" set NEED_LINK=1
        if "%BUILD_DIR%\test\white\infra\test_memory_pool.o" gtr "%BUILD_DIR%\test\white\infra\test_memory_pool.exe" set NEED_LINK=1
        if "%BUILD_DIR%\test\white\infra\test_memory_pool_main.o" gtr "%BUILD_DIR%\test\white\infra\test_memory_pool.exe" set NEED_LINK=1
        if "%BUILD_DIR%\infra\libinfra.a" gtr "%BUILD_DIR%\test\white\infra\test_memory_pool.exe" set NEED_LINK=1
        
        if !NEED_LINK!==1 (
            echo Linking test_memory_pool...
            "%GCC%" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\infra\test_memory_pool.o" "%BUILD_DIR%\test\white\infra\test_memory_pool_main.o" "%BUILD_DIR%\infra\libinfra.a" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test\white\infra\test_memory_pool.exe.dbg"
            if errorlevel 1 (
                echo Failed to link test_memory_pool
                exit /b 1
            )

            echo Creating binary test_memory_pool...
            "%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\white\infra\test_memory_pool.exe.dbg" "%BUILD_DIR%\test\white\infra\test_memory_pool.exe"
            if errorlevel 1 (
                echo Failed to create binary test_memory_pool
                exit /b 1
            )
        ) else (
            echo test_memory_pool binary is up to date.
        )

        if not "%2"=="norun" (
            echo Running test_memory_pool tests...
            "%BUILD_DIR%\test\white\infra\test_memory_pool.exe"
            echo.
        )
    ) else (
        set NEED_LINK=0
        if not exist "%BUILD_DIR%\test\white\infra\%%~nf.exe" set NEED_LINK=1
        if "%BUILD_DIR%\test\white\infra\%%~nf.o" gtr "%BUILD_DIR%\test\white\infra\%%~nf.exe" set NEED_LINK=1
        if "%BUILD_DIR%\infra\libinfra.a" gtr "%BUILD_DIR%\test\white\infra\%%~nf.exe" set NEED_LINK=1
        
        if !NEED_LINK!==1 (
            echo Linking %%~nf...
            "%GCC%" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\infra\%%~nf.o" "%BUILD_DIR%\infra\libinfra.a" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test\white\infra\%%~nf.exe.dbg"
            if errorlevel 1 (
                echo Failed to link %%~nf
                exit /b 1
            )

            echo Creating binary %%~nf...
            "%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\white\infra\%%~nf.exe.dbg" "%BUILD_DIR%\test\white\infra\%%~nf.exe"
            if errorlevel 1 (
                echo Failed to create binary %%~nf
                exit /b 1
            )
        ) else (
            echo %%~nf binary is up to date.
        )

        if not "%2"=="norun" (
            echo Running %%~nf tests...
            "%BUILD_DIR%\test\white\infra\%%~nf.exe"
            echo.
        )
    )
)

echo Build complete.

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

echo Total build time: %hours%:%mins%:%secs%.%ms% (HH:MM:SS.MS) 

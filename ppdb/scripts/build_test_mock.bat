@echo off

call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building infra library first...
call "%~dp0\build_infra.bat"
if errorlevel 1 exit /b 1

echo Building test framework...
if not exist "%BUILD_DIR%\test\white\framework" mkdir "%BUILD_DIR%\test\white\framework"
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\test_framework.c" -c -o "%BUILD_DIR%\test\white\framework\test_framework.o"
if errorlevel 1 exit /b 1

echo Building mock framework...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\mock_framework.c" -c -o "%BUILD_DIR%\test\white\framework\mock_framework.o"
if errorlevel 1 exit /b 1

echo Building memory mock...
if not exist "%BUILD_DIR%\test\white\infra" mkdir "%BUILD_DIR%\test\white\infra"
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\infra\mock_memory.c" -c -o "%BUILD_DIR%\test\white\infra\mock_memory.o"
if errorlevel 1 exit /b 1

echo Building test cases...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\infra\test_memory_mock.c" -c -o "%BUILD_DIR%\test\white\infra\test_memory_mock.o"
if errorlevel 1 exit /b 1

echo Linking test executables...
"%GCC%" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\infra\mock_memory.o" "%BUILD_DIR%\test\white\infra\test_memory_mock.o" "%BUILD_DIR%\infra\libinfra.a" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test\white\infra\test_memory_mock.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\white\infra\test_memory_mock.exe.dbg" "%BUILD_DIR%\test\white\infra\test_memory_mock.exe"
if errorlevel 1 exit /b 1

if not "%1"=="norun" (
  "%BUILD_DIR%\test\white\infra\test_memory_mock.exe"
)

echo Build complete. 
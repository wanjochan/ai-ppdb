@echo off
setlocal

rem Build environment setup
call "%~dp0build_env.bat"
if errorlevel 1 exit /b 1

rem Build infra library first...
echo Building infra library first...
call "%~dp0build_infra.bat"
if errorlevel 1 exit /b 1

rem Create build directories if they don't exist
if not exist "%BUILD_DIR%\test\white\mock" mkdir "%BUILD_DIR%\test\white\mock"
if not exist "%BUILD_DIR%\test\white\framework" mkdir "%BUILD_DIR%\test\white\framework"

rem Build test framework
echo Building test framework...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\test_framework.c" -c -o "%BUILD_DIR%\test\white\framework\test_framework.o"
if errorlevel 1 exit /b 1

rem Build mock framework
echo Building mock framework...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\mock_framework.c" -c -o "%BUILD_DIR%\test\white\framework\mock_framework.o"
if errorlevel 1 exit /b 1

rem Build core mock
echo Building core mock...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\mock\mock_core.c" -c -o "%BUILD_DIR%\test\white\mock\mock_core.o"
if errorlevel 1 exit /b 1

rem Build memory mock
echo Building memory mock...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\mock\mock_memory.c" -c -o "%BUILD_DIR%\test\white\mock\mock_memory.o"
if errorlevel 1 exit /b 1

rem Build test cases
echo Building test cases...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\test_core_mock.c" -c -o "%BUILD_DIR%\test\white\test_core_mock.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\test_memory_mock.c" -c -o "%BUILD_DIR%\test\white\test_memory_mock.o"
if errorlevel 1 exit /b 1

rem Link test executables
echo Linking test executables...
"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\white\test_core_mock.o" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\mock\mock_core.o" "%BUILD_DIR%\infra\libinfra.a" %LIBS% -o "%BUILD_DIR%\test\white\test_core_mock.exe"
if errorlevel 1 exit /b 1

"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\white\test_memory_mock.o" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\mock\mock_memory.o" "%BUILD_DIR%\infra\libinfra.a" %LIBS% -o "%BUILD_DIR%\test\white\test_memory_mock.exe"
if errorlevel 1 exit /b 1

echo Build complete.
exit /b 0 
@echo off
setlocal EnableDelayedExpansion

rem Load environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building peer layer...

rem Build peer layer
%CC% %CFLAGS% -c ^
%SRC_DIR%\peer\peer.c ^
%SRC_DIR%\peer\peer_server.c ^
%SRC_DIR%\peer\peer_client.c ^
%SRC_DIR%\peer\peer_proto.c ^
%SRC_DIR%\peer\peer_conn.c ^
%SRC_DIR%\peer\peer_memcached.c ^
-o %BUILD_DIR%\peer.o
if errorlevel 1 exit /b 1

rem Run tests if not disabled
if not "%1"=="notest" (
    call "%~dp0\build_test.bat"
    if errorlevel 1 exit /b 1
    echo Running peer tests...
    %BUILD_DIR%\test.exe peer
    if errorlevel 1 exit /b 1
)

echo Peer layer build completed 
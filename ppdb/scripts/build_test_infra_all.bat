set "SCRIPT_DIR=%~dp0"

call %SCRIPT_DIR%\build_test_infra.bat log
call %SCRIPT_DIR%\build_test_infra.bat memory
call %SCRIPT_DIR%\build_test_infra.bat memory_pool
call %SCRIPT_DIR%\build_test_infra.bat struct
call %SCRIPT_DIR%\build_test_infra.bat error
call %SCRIPT_DIR%\build_test_infra.bat sync

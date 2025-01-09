@echo off
setlocal

rem Set environment
call build_env.bat

rem Build and run infra tests
echo Building and running infra tests...
call build_infra.bat test

rem Build and run other test suites here...

echo All tests complete! 
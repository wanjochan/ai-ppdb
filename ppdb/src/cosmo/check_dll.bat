@echo off
echo Checking test4.dll...

REM 使用 PowerShell 获取文件信息
powershell -Command "Get-Item test4.dll | Format-List"

REM 使用 PowerShell 检查是否为 PE 文件
powershell -Command "$bytes = Get-Content test4.dll -Encoding Byte -TotalCount 2; if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) { Write-Host 'Valid PE file (MZ signature found)' } else { Write-Host 'Not a valid PE file' }"

REM 使用 strings 命令查找导出函数（如果有 strings 命令的话）
if exist "C:\Program Files\Git\usr\bin\strings.exe" (
    echo.
    echo Searching for exported function names:
    "C:\Program Files\Git\usr\bin\strings.exe" test4.dll | findstr "test4_func"
) 
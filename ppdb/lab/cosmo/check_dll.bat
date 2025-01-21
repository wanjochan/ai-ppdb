@echo off
echo Checking test4.dll...

REM 使用 PowerShell 获取文件信息
powershell -Command "Get-Item test4.dll | Format-List"

REM 使用 PowerShell 检查是否为 ELF 文件
powershell -Command "$bytes = Get-Content test4.dll -Encoding Byte -TotalCount 4; if ($bytes[0] -eq 0x7F -and $bytes[1] -eq 0x45 -and $bytes[2] -eq 0x4C -and $bytes[3] -eq 0x46) { Write-Host 'Valid ELF file (ELF signature found)' } else { Write-Host 'Not a valid ELF file' }"

REM 使用 readelf 检查 ELF 信息（如果有的话）
if exist "C:\Program Files\Git\usr\bin\readelf.exe" (
    echo.
    echo ELF Header:
    "C:\Program Files\Git\usr\bin\readelf.exe" -h test4.dll
    echo.
    echo Dynamic Symbols:
    "C:\Program Files\Git\usr\bin\readelf.exe" -s test4.dll | findstr "test4_func"
) 
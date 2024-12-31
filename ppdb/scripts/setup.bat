@echo off
setlocal enabledelayedexpansion

echo Checking development environment...

:: Get latest Cosmopolitan version
echo Fetching latest Cosmopolitan version...
if defined HTTPS_PROXY (
    echo Using proxy: %HTTPS_PROXY%
    for /f "tokens=2 delims=: " %%a in ('powershell -Command "& {$proxy = '%HTTPS_PROXY%'; $webClient = New-Object System.Net.WebClient; $webClient.Proxy = New-Object System.Net.WebProxy($proxy); $webClient.DownloadString('https://api.github.com/repos/jart/cosmopolitan/releases/latest') | findstr tag_name}"') do (
        set VERSION=%%~a
        set VERSION=!VERSION:"=!
        set VERSION=!VERSION:,=!
    )
) else (
    for /f "tokens=2 delims=: " %%a in ('powershell -Command "& {(Invoke-WebRequest -Uri 'https://api.github.com/repos/jart/cosmopolitan/releases/latest').Content | findstr tag_name}"') do (
        set VERSION=%%~a
        set VERSION=!VERSION:"=!
        set VERSION=!VERSION:,=!
    )
)
echo Latest version: !VERSION!
set COSMO_URL=https://github.com/jart/cosmopolitan/releases/download/!VERSION!/cosmopolitan-!VERSION!.zip
set CROSS9_URL=https://cosmo.zip/pub/cosmos/cross9/cross9.zip

:: Check if directories exist
if not exist "%~dp0..\..\cosmopolitan" (
    echo Downloading Cosmopolitan...
    if defined HTTPS_PROXY (
        powershell -Command "& {$proxy = '%HTTPS_PROXY%'; $webClient = New-Object System.Net.WebClient; $webClient.Proxy = New-Object System.Net.WebProxy($proxy); $webClient.DownloadFile('%COSMO_URL%', '%TEMP%\cosmo.zip')}"
    ) else (
        powershell -Command "& {Invoke-WebRequest -Uri '%COSMO_URL%' -OutFile '%TEMP%\cosmo.zip'}"
    )
    powershell -Command "& {Expand-Archive -Path '%TEMP%\cosmo.zip' -DestinationPath '%~dp0..\..' -Force}"
    del "%TEMP%\cosmo.zip"
    echo Cosmopolitan downloaded and extracted.
) else (
    echo Cosmopolitan directory already exists.
)

if not exist "%~dp0..\..\cross9" (
    echo Downloading Cross9...
    if defined HTTPS_PROXY (
        powershell -Command "& {$proxy = '%HTTPS_PROXY%'; $webClient = New-Object System.Net.WebClient; $webClient.Proxy = New-Object System.Net.WebProxy($proxy); $webClient.DownloadFile('%CROSS9_URL%', '%TEMP%\cross9.zip')}"
    ) else (
        powershell -Command "& {Invoke-WebRequest -Uri '%CROSS9_URL%' -OutFile '%TEMP%\cross9.zip'}"
    )
    powershell -Command "& {Expand-Archive -Path '%TEMP%\cross9.zip' -DestinationPath '%~dp0..\..\cross9' -Force}"
    del "%TEMP%\cross9.zip"
    echo Cross9 downloaded and extracted.
) else (
    echo Cross9 directory already exists.
)

echo Setup completed.
endlocal 
@echo off
chcp 65001 > nul
setlocal

REM Set proxy
set HTTP_PROXY=http://127.0.0.1:8889
set HTTPS_PROXY=http://127.0.0.1:8889

REM Download tinycc source
echo Downloading tinycc...
cd /d d:\dev\ai-ppdb
curl -L -o tinycc.zip https://github.com/TinyCC/tinycc/archive/refs/heads/mob.zip

REM Extract to tinycc_src
echo Extracting to tinycc_src...
powershell -Command "Expand-Archive -Force tinycc.zip -DestinationPath .\tinycc_src"

REM Copy to cosmo_tinycc
echo Copying to cosmo_tinycc...
xcopy /E /I /Y .\tinycc_src\tinycc-mob .\cosmo_tinycc\

REM Cleanup
echo Cleaning up...
del tinycc.zip

echo Done!

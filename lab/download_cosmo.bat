@echo off
chcp 65001 > nul
setlocal

REM Set proxy
set HTTP_PROXY=http://127.0.0.1:8889
set HTTPS_PROXY=http://127.0.0.1:8889

REM Download cosmopolitan
echo Downloading cosmopolitan...
cd /d d:\dev\ai-ppdb
curl -L -o cosmo.zip https://github.com/jart/cosmopolitan/archive/refs/heads/master.zip

REM Extract
echo Extracting...
powershell -Command "Expand-Archive -Force cosmo.zip -DestinationPath ."

REM Move to correct location
echo Moving files...
move cosmopolitan-master cosmopolitan

REM Cleanup
echo Cleaning up...
del cosmo.zip

echo Done!

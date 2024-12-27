@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

REM Set directories
set ROOT_DIR=d:\dev\ai-ppdb
set TCC_DIR=%ROOT_DIR%\cosmo_tinycc
set LAB_DIR=%ROOT_DIR%\lab

REM Apply patch
cd /d %TCC_DIR%
patch -p1 < "%LAB_DIR%\tinycc-cosmo.patch"

echo Patch applied!

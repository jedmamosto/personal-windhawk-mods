@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Building Windhawk Clang++ Pre-Compiled Header (PCH)...
echo ========================================================

set "COMPILER=C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
set "INCLUDE_DIR=C:\Program Files\Windhawk\Compiler\include"
set "SRC_DIR=%~dp0"
if "%SRC_DIR:~-1%"=="\" set "SRC_DIR=%SRC_DIR:~0,-1%"

set "PCH_SRC=%SRC_DIR%\dynamic_island_pch.hpp"
set "PCH_OUT=%SRC_DIR%\dynamic_island_pch.hpp.pch"

if not exist "%COMPILER%" (
    echo [ERROR] Windhawk compiler not found at: "%COMPILER%"
    exit /b 1
)

if not exist "%PCH_SRC%" (
    echo [ERROR] PCH source header not found at: "%PCH_SRC%"
    exit /b 1
)

echo Source Header : %PCH_SRC%
echo Output Target : %PCH_OUT%
echo.

"%COMPILER%" -x c++-header -std=c++20 -DUNICODE -D_UNICODE -DWH_MOD -DWH_MOD_ID=L\"dynamic-island-jedmamosto-fork\" -I"%INCLUDE_DIR%" -I"%SRC_DIR%" "%PCH_SRC%" -o "%PCH_OUT%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo [SUCCESS] Pre-compiled header successfully generated!
    echo Target: %PCH_OUT%
    echo ========================================================
    exit /b 0
) else (
    echo.
    echo ========================================================
    echo [FAILED] Pre-compiled header generation failed with code %ERRORLEVEL%
    echo ========================================================
    exit /b %ERRORLEVEL%
)

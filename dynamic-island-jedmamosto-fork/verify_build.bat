@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Verifying Dynamic Island compilation with Windhawk Clang++...
echo ========================================================

set "COMPILER=C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
set "INCLUDE_DIR=C:\Program Files\Windhawk\Compiler\include"
set "SRC_DIR=%~dp0"
if "%SRC_DIR:~-1%"=="\" set "SRC_DIR=%SRC_DIR:~0,-1%"

set "MOD_SRC=%SRC_DIR%\dynamic-island-jedmamosto-fork.wh.cpp"
set "PCH_FILE=%SRC_DIR%\dynamic_island_pch.hpp.pch"

if not exist "%COMPILER%" (
    echo [ERROR] Windhawk compiler not found at: "%COMPILER%"
    exit /b 1
)

set "PCH_FLAG="
if exist "%PCH_FILE%" (
    echo [PCH] Fast pre-compiled header detected: %PCH_FILE%
    set "PCH_FLAG=-include-pch "%PCH_FILE%""
) else (
    echo [FALLBACK] PCH not found. Using full AST parse fallback. Run build_pch.bat to accelerate.
)

"%COMPILER%" -fsyntax-only -std=c++20 -DUNICODE -D_UNICODE -DWH_MOD -DWH_MOD_ID=L\"dynamic-island-jedmamosto-fork\" %PCH_FLAG% -I"%INCLUDE_DIR%" -I"%SRC_DIR%" -include "windhawk_api.h" "%MOD_SRC%"

set "BUILD_STATUS=%ERRORLEVEL%"
if %BUILD_STATUS% EQU 0 (
    echo [SUCCESS] Mod syntax verified with 0 errors!
) else (
    echo [FAILED] Compilation failed with error code %BUILD_STATUS%
)

exit /b %BUILD_STATUS%

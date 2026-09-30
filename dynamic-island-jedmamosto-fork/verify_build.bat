@echo off
echo Verifying Dynamic Island compilation with Windhawk Clang++...
"C:\Program Files\Windhawk\Compiler\bin\clang++.exe" -fsyntax-only -std=c++20 -DUNICODE -D_UNICODE -DWH_MOD -DWH_MOD_ID=L\"dynamic-island-jedmamosto-fork\" -I"C:\Program Files\Windhawk\Compiler\include" -I"%~dp0." -include "windhawk_api.h" "%~dp0dynamic-island-jedmamosto-fork.wh.cpp"
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Compilation verified with 0 errors!
) else (
    echo [FAILED] Compilation failed with error code %ERRORLEVEL%
)
pause

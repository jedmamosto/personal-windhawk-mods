@echo off
setlocal
echo [Direct2D AGY Snapshot] Compiling standalone AGY snapshot tool...
"C:\Program Files\Windhawk\Compiler\bin\clang++.exe" -std=c++20 -DUNICODE -D_UNICODE -mconsole -municode -I"C:\Program Files\Windhawk\Compiler\include" "%~dp0.tools\render_agy_snapshot.cpp" -ld2d1 -ldwrite -lwindowscodecs -lole32 -loleaut32 -o "%~dp0.tools\render_agy_snapshot.exe"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile render_agy_snapshot.exe
    exit /b %ERRORLEVEL%
)

echo [Direct2D AGY Snapshot] Executing offscreen Direct2D render target...
pushd "%~dp0"
".tools\render_agy_snapshot.exe"
popd

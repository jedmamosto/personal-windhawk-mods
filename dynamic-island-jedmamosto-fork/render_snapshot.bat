@echo off
setlocal
echo [Direct2D Snapshot] Compiling standalone snapshot tool...
"C:\Program Files\Windhawk\Compiler\bin\clang++.exe" -std=c++20 -DUNICODE -D_UNICODE -mconsole -municode -I"C:\Program Files\Windhawk\Compiler\include" "%~dp0.tools\render_snapshot.cpp" -ld2d1 -ldwrite -lwindowscodecs -lole32 -loleaut32 -o "%~dp0.tools\render_snapshot.exe"
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to compile render_snapshot.exe
    exit /b %ERRORLEVEL%
)

echo [Direct2D Snapshot] Executing offscreen render target...
pushd "%~dp0"
".tools\render_snapshot.exe"
popd

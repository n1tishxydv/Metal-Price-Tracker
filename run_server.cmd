@echo off
cd /d "%~dp0"
if not exist server.exe (
    call build_msvc.cmd
    if errorlevel 1 exit /b 1
)
echo Starting backend on http://localhost:8080
server.exe

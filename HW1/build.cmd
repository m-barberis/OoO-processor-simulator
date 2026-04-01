@echo off
setlocal

cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
	echo g++ was not found on PATH. Install MinGW-w64, MSYS2, WSL, or use the course Docker environment.
	exit /b 1
)

g++ -std=c++17 -O2 -Wall -Wextra -I. src\*.cpp -o build.exe
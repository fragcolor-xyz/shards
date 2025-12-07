@echo off
:: setup-windows.bat
:: Launcher for setup-windows.ps1 - handles execution policy
:: Run this as Administrator

echo === Shards Windows Setup Launcher ===
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator
    echo Right-click this file and select "Run as administrator"
    pause
    exit /b 1
)

:: Get the directory where this script is located
set SCRIPT_DIR=%~dp0

:: Run PowerShell script with bypass execution policy
powershell.exe -ExecutionPolicy Bypass -NoProfile -File "%SCRIPT_DIR%setup-windows.ps1"

pause

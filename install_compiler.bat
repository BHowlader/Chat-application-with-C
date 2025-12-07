@echo off
echo Installing C Compiler (WinLibs MinGW-w64)...
echo This may take a few minutes. Please accept any prompts.

winget install --id=BrechtSanders.WinLibs.POSIX.UCRT -e --accept-source-agreements

if %errorlevel% neq 0 (
    echo Installation failed or was cancelled.
    echo Please install MinGW manually or try running this script as Administrator.
    pause
    exit /b 1
)

echo.
echo Installation completed!
echo IMPORTANT: You may need to RESTART your terminal or computer for the 'gcc' command to be recognized.
echo.
pause

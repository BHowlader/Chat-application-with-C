@echo off
echo Compiling Server...
gcc server.c -o server.exe -lws2_32
if %errorlevel% neq 0 (
    echo Server compilation failed!
    exit /b 1
)

echo Compiling Client...
gcc client.c -o client.exe -lws2_32
if %errorlevel% neq 0 (
    echo Client compilation failed!
    exit /b 1
)

echo Compilation Successful!
echo 1. Run 'server.exe'
echo 2. Run 'client.exe' (multiple instances for testing)
echo Default users: admin/admin123, user1/pass1

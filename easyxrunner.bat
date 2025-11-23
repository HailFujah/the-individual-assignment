@echo off

set COMPILER=D:\TDM-GCC\bin\g++.exe

if "%~1"=="" (
    echo Error: please provide your source file.
    echo Example: easyx.bat "C:\Users\You\Desktop\test.cpp"
    pause
    exit /b
)

set "SOURCE=%~1"
set "NAME=%~n1"
set "DIR=%~dp1"

taskkill /f /im "%NAME%.exe" >nul 2>&1

echo Compiling: %SOURCE%

set "OUTPUT=%DIR%%NAME%.exe"

if exist "%OUTPUT%" (
    del /f /q "%OUTPUT%"
)

:: 添加了必要的库链接参数：-lwininet -lgdi32 -lcomctl32 -lws2_32
"%COMPILER%" -fdiagnostics-color=always -g "%SOURCE%" -o "%OUTPUT%" -leasyx -lwininet -lgdi32 -lcomctl32 -lws2_32

if errorlevel 1 (
    echo Compilation failed!
    pause
    exit /b
)

echo Compilation succeeded. Running program...
echo ------------------------------
cd /d "%DIR%"
"%OUTPUT%"
echo ------------------------------
pause
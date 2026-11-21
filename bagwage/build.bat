@echo off
setlocal

rem Put Qt 5.9.1 first in PATH so MSYS2 Qt6 DLLs are never loaded.
set PATH=D:\Qt5.9.1\5.9.1\mingw53_32\bin;D:\Qt5.9.1\Tools\mingw530_32\bin;%PATH%

cd /d "%~dp0"

if /i "%1"=="run" goto run
if /i "%1"=="clean" goto clean

echo [1/2] qmake ...
qmake bagwage.pro
if errorlevel 1 goto fail

echo [2/2] mingw32-make ...
mingw32-make -j4
if errorlevel 1 goto fail

echo.
echo Build OK. Run: build.bat run
exit /b 0

:run
if not exist bagwage.exe (
    echo bagwage.exe not found. Run build.bat first.
    exit /b 1
)
bagwage.exe %2 %3
exit /b %errorlevel%

:clean
mingw32-make distclean
del /q bagwage.exe 2>nul
echo Cleaned build outputs. bagwage.db kept.
exit /b 0

:fail
echo.
echo Build FAILED. Copy the errors above.
exit /b 1
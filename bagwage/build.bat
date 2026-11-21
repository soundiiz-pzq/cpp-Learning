@echo off
REM ------------------------------------------------------------------
REM  bagwage build helper.  Keep this file ASCII-only and CRLF.
REM  Chinese text here is unsafe: cmd decodes batch files with the OEM
REM  code page (936) and a mangled byte may turn into & or | which
REM  splits the line and can re-invoke this script in a loop.
REM ------------------------------------------------------------------
chcp 65001 >nul
setlocal

REM Put Qt 5.9.1 first so we never pick up the Qt 6 DLLs from msys64
set PATH=D:\Qt5.9.1\5.9.1\mingw53_32\bin;D:\Qt5.9.1\Tools\mingw530_32\bin;%PATH%

cd /d "%~dp0"

if /i "%~1"=="run"   goto run
if /i "%~1"=="test"  goto test
if /i "%~1"=="clean" goto clean

echo [1/2] qmake ...
qmake bagwage.pro
if errorlevel 1 goto fail

echo [2/2] mingw32-make ...
mingw32-make -j4
if errorlevel 1 goto fail

echo.
echo Build OK. Next: build.bat run   /   build.bat test
exit /b 0

:run
if not exist bagwage.exe (
    echo bagwage.exe not found. Run build.bat first.
    exit /b 1
)
bagwage.exe %2 %3
exit /b %errorlevel%

:test
if not exist bagwage.exe (
    echo bagwage.exe not found. Run build.bat first.
    exit /b 1
)
bagwage.exe --selftest
if errorlevel 1 (
    echo.
    echo SELF TEST FAILED - see the [FAIL] lines above.
    exit /b 1
)
echo.
echo Self test passed.
exit /b 0

:clean
mingw32-make distclean
if exist bagwage.exe del /q bagwage.exe
echo Build artifacts removed ^(bagwage.db kept^).
exit /b 0

:fail
echo.
echo BUILD FAILED - paste the errors above.
exit /b 1

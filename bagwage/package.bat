@echo off
REM ------------------------------------------------------------------
REM  bagwage packaging helper.  Keep this file ASCII-only and CRLF.
REM  Builds a windowless release into ..\..\bagwage-dist and runs
REM  windeployqt, then copies the Chinese translations that
REM  windeployqt does NOT ship for Qt 5.9.1.
REM ------------------------------------------------------------------
chcp 65001 >nul
setlocal

set PATH=D:\Qt5.9.1\5.9.1\mingw53_32\bin;D:\Qt5.9.1\Tools\mingw530_32\bin;%PATH%
set QTDIR=D:\Qt5.9.1\5.9.1\mingw53_32
set BUILD=%~dp0.package-build
set DIST=%~dp0..\..\bagwage-dist

cd /d "%~dp0"

echo [1/4] qmake (CONFIG+=package, no console window) ...
if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%"
qmake "CONFIG+=package" "%~dp0bagwage.pro"
if errorlevel 1 goto fail

echo [2/4] mingw32-make ...
mingw32-make -j4
if errorlevel 1 goto fail
popd

echo [3/4] deploy to "%DIST%" ...
if not exist "%DIST%" mkdir "%DIST%"
copy /y "%BUILD%\bagwage.exe" "%DIST%\bagwage.exe" >nul
pushd "%DIST%"
windeployqt --release --compiler-runtime --no-opengl-sw bagwage.exe
if errorlevel 1 goto fail

echo [4/4] Chinese translations + trim unused drivers ...
if not exist translations mkdir translations
del /q translations\*.qm 2>nul
if exist "%QTDIR%\translations\qt_zh_CN.qm" copy /y "%QTDIR%\translations\qt_zh_CN.qm" translations\ >nul
if exist "%QTDIR%\translations\qt_zh_TW.qm" copy /y "%QTDIR%\translations\qt_zh_TW.qm" translations\ >nul
del /q sqldrivers\qsqlmysql.dll sqldrivers\qsqlodbc.dll sqldrivers\qsqlpsql.dll 2>nul
popd

echo.
echo Package ready: %DIST%
echo Run it there; bagwage.db will be created next to the exe.
exit /b 0

:fail
popd 2>nul
echo.
echo PACKAGING FAILED - paste the errors above.
exit /b 1

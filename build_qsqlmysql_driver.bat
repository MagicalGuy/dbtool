@echo off
rem ============================================================
rem  Build the Qt QMYSQL driver plugin (Release, x64, MSVC2015)
rem  against MySQL 8.0 client development files.
rem ============================================================
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
if errorlevel 1 exit /b 1

set "QMAKE=D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin\qmake.exe"
set "JOM=D:\Qt\Qt5.13.0\Tools\QtCreator\bin\jom.exe"
set "SQLDRV=D:\Qt\Qt5.13.0\5.13.0\Src\qtbase\src\plugins\sqldrivers"
set "MYSQL=D:\mysql-dev\mysql-8.0.46-winx64"

cd /d "%SQLDRV%"
if errorlevel 1 exit /b 1

"%QMAKE%" -- "MYSQL_INCDIR=%MYSQL%/include" "MYSQL_LIBDIR=%MYSQL%/lib"
if errorlevel 1 exit /b 1

"%JOM%" sub-mysql
if errorlevel 1 exit /b 1

echo QSQLMYSQL_BUILD_OK
endlocal

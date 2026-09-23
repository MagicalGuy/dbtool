@echo off
rem ============================================================
rem  Build DbGuiTool  (Qt 5.13.0 MSVC2015 64-bit, Debug)
rem  Shadow build into a separate build folder.
rem ============================================================
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat"
set "QMAKE=D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin\qmake.exe"
set "JOM=D:\Qt\Qt5.13.0\Tools\QtCreator\bin\jom.exe"
set "SRC=E:\QT course\C++QT5\01\003dbtool\dbtool.pro"
set "BUILD=E:\QT course\C++QT5\01\build-dbtool-Desktop_Qt_5_13_0_MSVC2015_64bit-Debug"

if not exist "%BUILD%" mkdir "%BUILD%"

call "%VCVARS%" x64
if errorlevel 1 exit /b 1

cd /d "%BUILD%"
if errorlevel 1 exit /b 1

"%QMAKE%" "%SRC%" -spec win32-msvc "CONFIG+=debug"
if errorlevel 1 exit /b 1

"%JOM%"
if errorlevel 1 exit /b 1

echo BUILD_OK
endlocal

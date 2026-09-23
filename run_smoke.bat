@echo off
rem Smoke launch: set up MSVC + Qt environment, then start the GUI app.
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64 >nul
set "PATH=D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin;%PATH%"
start "dbtool-smoke" "E:\QT course\C++QT5\01\build-dbtool-Desktop_Qt_5_13_0_MSVC2015_64bit-Debug\debug\dbtool.exe"

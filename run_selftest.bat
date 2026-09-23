@echo off
rem ============================================================
rem  Run DbGuiTool headless self-test (offscreen platform).
rem  Verifies SQLite driver, in-memory CRUD and MainWindow build.
rem ============================================================
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x64
set "PATH=D:\Qt\Qt5.13.0\5.13.0\msvc2015_64\bin;%PATH%"
set QT_QPA_PLATFORM=offscreen

"E:\QT course\C++QT5\01\build-dbtool-Desktop_Qt_5_13_0_MSVC2015_64bit-Debug\debug\dbtool.exe" --selftest
set RC=%errorlevel%
echo SELFTEST_EXIT_CODE=%RC%
endlocal & exit /b %RC%

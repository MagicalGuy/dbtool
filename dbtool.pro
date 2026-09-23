#-------------------------------------------------
#  DbGuiTool - 一个类似 Navicat 的 Qt 数据库 GUI 工具（初始版本）
#  支持：连接管理、表数据增删改查、SQL 查询执行、结果导出
#  默认内置 SQLite 驱动，开箱即用；同时支持系统中已安装的其它
#  Qt 数据库驱动（如 QODBC、QPSQL）。
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += c++11

TARGET = dbtool
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    connection.cpp \
    dbmanager.cpp \
    connectiondialog.cpp \
    tabletab.cpp \
    querytab.cpp \
    mainwindow.cpp

HEADERS += \
    connection.h \
    dbmanager.h \
    connectiondialog.h \
    tabletab.h \
    querytab.h \
    mainwindow.h

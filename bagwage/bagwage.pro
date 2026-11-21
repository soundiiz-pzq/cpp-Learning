#-------------------------------------------------------------------
# 书包生产记账系统 (bagwage)
#
# 工具链：Qt 5.9.1 / MinGW 5.3 (32bit) / qmake
# 注意：本工程必须用 qmake(moc/uic) 构建，不能用单文件 g++ 直接编译。
#-------------------------------------------------------------------

QT += core gui widgets sql charts

CONFIG += c++11 console release
CONFIG -= app_bundle debug_and_release

TEMPLATE = app
TARGET   = bagwage

# 源码按层放进子目录；INCLUDEPATH 只加工程根目录，
# include 时写全相对路径（如 "db/dbmanager.h"），避免同名头文件撞车。
INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    db/dbmanager.cpp \
    dao/employeedao.cpp \
    service/employeeservice.cpp \
    ui/mainwindow.cpp \
    ui/employeedialog.cpp \
    ui/placeholderpage.cpp

HEADERS += \
    dao/models.h \
    db/dbmanager.h \
    dao/employeedao.h \
    service/employeeservice.h \
    ui/mainwindow.h \
    ui/employeedialog.h \
    ui/placeholderpage.h

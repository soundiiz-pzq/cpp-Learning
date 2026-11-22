#-------------------------------------------------------------------
# 书包生产记账系统 (bagwage)
#
# 工具链：Qt 5.9.1 / MinGW 5.3 (32bit) / qmake
# 注意：本工程必须用 qmake(moc/uic) 构建，不能用单文件 g++ 直接编译。
#-------------------------------------------------------------------

QT += core gui widgets sql charts

CONFIG += c++11 release
CONFIG -= app_bundle debug_and_release

# 开发构建带控制台，方便看 --selftest 的输出；
# 打包时用 qmake "CONFIG+=package" 去掉黑框（GUI 程序不要控制台）
!contains(CONFIG, package): CONFIG += console

TEMPLATE = app
TARGET   = bagwage

# 源码按层放进子目录；INCLUDEPATH 只加工程根目录，
# include 时写全相对路径（如 "db/dbmanager.h"），避免同名头文件撞车。
INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    selftest.cpp \
    db/dbmanager.cpp \
    db/transaction.cpp \
    dao/bagdao.cpp \
    dao/bagprocessdao.cpp \
    dao/employeedao.cpp \
    dao/orderdao.cpp \
    dao/orderdetaildao.cpp \
    dao/processdao.cpp \
    dao/wagedao.cpp \
    service/bagservice.cpp \
    service/chartworker.cpp \
    service/employeeservice.cpp \
    service/monthcatalog.cpp \
    service/orderservice.cpp \
    service/processservice.cpp \
    service/wageservice.cpp \
    ui/mainwindow.cpp \
    ui/bagdialog.cpp \
    ui/chartpage.cpp \
    ui/employeedialog.cpp \
    ui/orderdialog.cpp \
    ui/processdialog.cpp \
    ui/placeholderpage.cpp

HEADERS += \
    selftest.h \
    dao/models.h \
    dao/bagdao.h \
    dao/bagprocessdao.h \
    db/dbmanager.h \
    db/transaction.h \
    dao/employeedao.h \
    dao/orderdao.h \
    dao/orderdetaildao.h \
    dao/processdao.h \
    dao/wagedao.h \
    service/serviceresult.h \
    service/bagservice.h \
    service/chartworker.h \
    service/employeeservice.h \
    service/monthcatalog.h \
    service/orderservice.h \
    service/processservice.h \
    service/wageservice.h \
    ui/mainwindow.h \
    ui/bagdialog.h \
    ui/chartpage.h \
    ui/employeedialog.h \
    ui/orderdialog.h \
    ui/processdialog.h \
    ui/placeholderpage.h

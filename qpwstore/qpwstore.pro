#-------------------------------------------------
#
# Project created by QtCreator 2014-04-01T19:43:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qpwstore
TEMPLATE = app

HEADERS += key_handler.hh list_entry.hh main_window.hh ../pwstore.hh ../pwstore_api_cxx.hh
SOURCES += key_handler.cc list_entry.cc main.cc main_window.cc ../pwstore.cc ../pwstore_api_cxx.cc

CONFIG += c++11
LIBS += -lssl -lcrypto

win32 {
LIBS += -lgdi32 -lws2_32
QMAKE_CXXFLAGS +=-DNO_GOOD
QMAKE_INCDIR_QT += -I/usr/i686-w64-mingw32/sys-root/mingw/include/qt5/ -I/usr/i686-w64-mingw32/sys-root/mingw/include/qt5/QtWidgets -I/usr/i686-w64-mingw32/sys-root/mingw/include/qt5/QtGui -I/usr/i686-w64-mingw32/sys-root/mingw/include/qt5/QtCore
#QMAKE_LFLAGS+=-L/usr/i686-w64-mingw32/sys-root/mingw/lib/
#QMAKE_LFLAGS+=-L/usr/i686-w64-mingw32/sys-root/mingw/bin/
}
unix {
QMAKE_CXX = clang++
}

QMAKE_CXXFLAGS += -Werror -Wall -Wextra -Wpedantic -Wpointer-arith -Wcast-align -Wredundant-decls -Wdisabled-optimization -Wno-long-long -Wwrite-strings -pedantic # -pedantic-errors -m64
QMAKE_INCDIR_QT += ../ ../../ #include_dirs
QMAKE_CXXFLAGS += $$join(QMAKE_INCDIR_QT, " -isystem", "-isystem")

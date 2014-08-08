#-------------------------------------------------
#
# Project created by QtCreator 2014-04-01T19:43:50
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = qpwstore
TEMPLATE = app

HEADERS += list_entry.hh main_window.hh ../pwstore.hh ../pwstore_api_cxx.hh
SOURCES += list_entry.cc main.cc main_window.cc ../pwstore.cc ../pwstore_api_cxx.cc

LIBS += -lssl -lcrypto #../bin/cam_app.a -L/opt/opencv-2.4.6/lib/ -lopencv_highgui -lopencv_core -lopencv_imgproc
CONFIG += c++11
QMAKE_CXX = clang++
QMAKE_LIB=lvm-ld -link-as-library -o
QMAKE_CXXFLAGS += -Werror -Wall -Wextra -Wpedantic -Wpointer-arith -Wcast-align -Wredundant-decls -Wdisabled-optimization -Wno-long-long -Wwrite-strings -pedantic # -pedantic-errors -m64
QMAKE_INCDIR_QT += ../ ../.deps/ include_dirs
QMAKE_CXXFLAGS += $$join(QMAKE_INCDIR_QT, " -isystem", "-isystem")

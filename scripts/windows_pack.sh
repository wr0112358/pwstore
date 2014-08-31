#!/bin/bash

function install_pwstore()
{
    local TMP=$(mktemp -d)
    local dir=$(pwd)
    cp /usr/x86_64-w64-mingw32/sys-root/mingw/bin/*.dll $TMP
    cp pwstore.exe $TMP
    cd $TMP
    #tar -czvf pwstore.tar \
    #    libcrypto-10.dll \
    #    libssl-10.dll \
    #    zlib1.dll \
    #    pwstore.exe

    tar -czvf pwstore.tar \
	*.dll \
	pwstore.exe
    mv pwstore.tar $dir
    cd $dir
    rm -rf $TMP
}

function install_qpwstore()
{
    local TMP=$(mktemp -d)
    local dir=$(pwd)
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libcrypto-10.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libssl-10.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/Qt5Widgets.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/Qt5Gui.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/Qt5Core.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libgcc_s_sjlj-1.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libstdc++-6.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libpcre16-0.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/zlib1.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libpng16-16.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libGLESv2.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libEGL.dll $TMP
    cp /usr/i686-w64-mingw32/sys-root/mingw/bin/libintl-8.dll $TMP

    mkdir $TMP/platforms
    cp /usr/i686-w64-mingw32/sys-root/mingw/lib/qt5/plugins/platforms/qwindows.dll $TMP/platforms


    cp qpwstore/release/qpwstore.exe $TMP
    cd $TMP

    tar -czvf qpwstore.tar \
	*.dll \
	platforms \
	qpwstore.exe

    mv qpwstore.tar $dir
    cd $dir
    rm -rf $TMP
}

install_pwstore
install_qpwstore


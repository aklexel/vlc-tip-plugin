#!/bin/bash
# Script to build a vlc plugin for Linux and Windows, for both 32-bit and 64-bit versions.
OS=${1:-ALL}
BITNESS=${2:-ALL}
shopt -s nocasematch

build_for_linux() {
    local BITNESS=$1

    if [[ "$BITNESS" == "ALL" ]]; then
        build_for_linux 32
        build_for_linux 64
        return
    fi

    echo -e "\nBuild a plugin for Linux $BITNESS-bit..."
    cd /plugin
    make clean
    make CC="cc -m$BITNESS"
    cp *.so build/linux/$BITNESS/
}

build_for_windows() {
    local BITNESS=$1
    declare -A PREFIX; PREFIX=( [32]="i686" [64]="x86_64" )

    if [[ "$BITNESS" == "ALL" ]]; then
        build_for_windows 32
        build_for_windows 64
        return
    fi

    echo -e "\nBuild a plugin for Windows $BITNESS-bit..."
    cd /opt/vlc-*-win$BITNESS/vlc-*/sdk/
    sed -i "s|^prefix=.*|prefix=${PWD}|g" lib/pkgconfig/*.pc
    export PKG_CONFIG_PATH="${PWD}/lib/pkgconfig:$PKG_CONFIG_PATH"

    cd /plugin
    make clean
    make CC=${PREFIX[$BITNESS]}-w64-mingw32-gcc LD=${PREFIX[$BITNESS]}-w64-mingw32-ld OS=Windows_NT
    cp *.dll build/win/$BITNESS/
}


if [[ ! "$OS" =~ ^(LINUX|WINDOWS|ALL)$ ]]; then
    echo "ERROR: Unsupported OS '$OS', please specify one of the following values: 'linux', 'windows' or 'all'"
    exit 1
fi

if [[ ! "$BITNESS" =~ ^(32|64|ALL)$ ]]; then
    echo "ERROR: Unsupported bitness '$BITNESS', please specify one of the following values: '32', '64' or 'all'"
    exit 1
fi

case $OS in
LINUX)
    build_for_linux $BITNESS
    ;;
WINDOWS)
    build_for_windows $BITNESS
    ;;
ALL)
    build_for_linux $BITNESS
    build_for_windows $BITNESS
    ;;
esac
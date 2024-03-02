#!/bin/bash

OS=$(uname -o 2>/dev/null)
if [[ $? -ne 0 ]] ; then
    # Older macOS/OSX versions of uname don't support -o
    OS=$(uname -s)
fi

function log {
    echo "[-- libcon4m --]" $@
}

function meson_build {
    if [[ ! -d ${1} ]]; then
        if [[ -f ${1} ]]; then
            rm -rf ${1}
        fi
        log Creating meson target ${1}
        meson setup ${@}
    fi
    cd ${1}

    log Compiling meson target ${1}
    if [[ ${OS} = "Darwin" ]] ; then
        meson compile
    else
        CC=musl-gcc meson compile
    fi

    log "Done!"
}

function usage {
    echo "Usage: build.sh [release | debug | clean]"
    exit 1
}

if [[ ${#} -eq 0 ]]; then
    meson_build build
elif [[ ${#} -ne 1 ]]; then
    usage

elif [[ ${1} == clean ]]; then
    for item in build debug release; do
        if [[ -d ${item} ]]; then
            log Cleaning ${item}
            cd ${item}
            meson compile --clean
            cd ..
        fi
    done
    log "Done!"

elif [[ ${1} == debug ]]; then
    meson_build debug --buildtype=debug
elif [[ ${1} == release ]]; then
    meson_build release --buildtype=release
else
    usage
fi

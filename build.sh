#! /bin/bash

TOOLCHAIN_DIR=../CybikoStuff/toolchain

rm -Rf CMakeFiles CMakeCache.txt
cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_DIR/xtreme.toolchain src
make clean
make

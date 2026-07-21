#! /bin/bash

TOOLCHAIN_DIR=../CybikoStuff/toolchain

cmake -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_DIR/xtreme.toolchain ./src/lcd/
make

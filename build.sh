#!/usr/bin/bash

set -e

gcc src/main.c $(cat src/hds.h | tr -d '"' | cut -d ' ' -f 2 | sed 's/\.h$/.c/') $(pkg-config --cflags --libs libsystemd)


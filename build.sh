#!/usr/bin/bash

set -e

gcc src/main.c src/bus_sh.c $(pkg-config --cflags --libs libsystemd) -o $1


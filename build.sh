#!/bin/bash

gcc src/main.c src/bus_sh.c -Wall -Wextra $2 -o $1 $(pkg-config --cflags --libs libsystemd)

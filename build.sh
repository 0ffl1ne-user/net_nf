#!/bin/bash

gcc src/main.c src/bus_sh.c -Wall -Wextra $(pkg-config --cflags --libs libsystemd) -o $1


#!/bin/bash

gcc src/main.c src/bus_sh.c $(pkg-config --cflags --libs libsystemd) -o $1


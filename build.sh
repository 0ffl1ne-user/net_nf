#!/bin/bash

set -e

gcc src/main.c src/bus_sh.c -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie -Wall -Wextra -Wl,-z,relro,-z,now $2 -o $1 $(pkg-config --cflags --libs libsystemd)

mv $1 /usr/local/bin/

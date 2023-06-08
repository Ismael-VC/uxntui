#!/bin/sh -e

RELEASE_FLAGS="-DNDEBUG -O2 -g0 -s"
DEBUG_FLAGS="-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined"
CORE_DEVICES="src/uxn.c src/devices/system.c src/devices/file.c src/devices/datetime.c -lpthread"
EMU_INC="${CORE_DEVICES} src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/uxn11.c -o bin/uxn11 -lX11"
CLI_INC="${CORE_DEVICES} src/uxncli.c -o bin/uxncli"

# find X11 libs on various systems
if [ -e /usr/X11R6 ]; then
	# OpenBSD
	RELEASE_FLAGS="-L/usr/X11R6/lib/ -I/usr/X11R6/include/ $RELEASE_FLAGS"
	DEBUG_FLAGS="-L/usr/X11R6/lib/ -I/usr/X11R6/include/ $DEBUG_FLAGS"
fi

if [ "${1}" = '--format' ];
then
	clang-format -i src/uxn11.c
	clang-format -i src/uxnasm.c
	clang-format -i src/uxncli.c
	clang-format -i src/devices/*
fi

rm -f bin/*
mkdir -p bin

cc ${RELEASE_FLAGS} src/uxnasm.c -o bin/uxnasm

if [ "${1}" = '--debug' ];
then
	cc ${C_FLAGS} ${LD_FLAGS} ${DEBUG_FLAGS} ${EMU_INC}
	cc ${C_FLAGS} ${LD_FLAGS} ${DEBUG_FLAGS} ${CLI_INC}
else
	cc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${EMU_INC}
	cc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${CLI_INC}
fi

if [ "${1}" = '--install' ];
then
	cp bin/uxn11 bin/uxnemu
	cp bin/uxnemu bin/uxnasm bin/uxncli $HOME/bin/
fi

# bin/uxnasm etc/polycat.tal bin/polycat.rom
# bin/uxn11 bin/polycat.rom
# bin/uxnasm etc/friend.tal bin/friend.rom

if [ "${1}" = '--norun' ]; then exit; fi

bin/uxnasm etc/mouse.tal bin/mouse.rom
bin/uxn11 bin/mouse.rom


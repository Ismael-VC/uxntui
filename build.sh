#!/bin/sh -e

RELEASE_FLAGS="-Os -DNDEBUG -g0 -s"
DEBUG_FLAGS="-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined"
EMU_INC="src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -o bin/uxn11 -lX11"
CLI_INC="src/uxn.c src/devices/system.c src/devices/file.c src/devices/datetime.c src/uxncli.c -o bin/uxncli"

# find X11 libs on various systems
if [ -e /usr/X11R6 ]; then
	# OpenBSD
	RELEASE_FLAGS="-L/usr/X11R6/lib/ -I/usr/X11R6/include/ $RELEASE_FLAGS"
	DEBUG_FLAGS="-L/usr/X11R6/lib/ -I/usr/X11R6/include/ $DEBUG_FLAGS"
fi


if [ "${1}" = '--format' ];
then
	echo "Formatting.."
	clang-format -i src/uxn.c
	clang-format -i src/uxn.h
	clang-format -i src/uxn11.c
	clang-format -i src/uxnasm.c
	clang-format -i src/uxncli.c
	clang-format -i src/devices/*
fi

echo "Cleaning.."
rm -f bin/*
mkdir -p bin

echo "Building.."
cc -DNDEBUG -Os -g0 -s src/uxnasm.c -o bin/uxnasm
if [ "${1}" = '--install' ];
then
	echo "Installing.."
	gcc ${RELEASE_FLAGS} ${EMU_INC}
	gcc ${RELEASE_FLAGS} ${CLI_INC}
	cp bin/uxnasm ~/bin
	cp bin/uxncli ~/bin
	cp bin/uxn11 ~/bin
else
	gcc ${DEBUG_FLAGS} ${EMU_INC}
	gcc ${DEBUG_FLAGS} ${CLI_INC} 
fi

echo "Assembling.."
bin/uxncli etc/drifblim.rom etc/polycat.tal && mv etc/polycat.rom bin/

echo "Running.."
bin/uxn11 ~/roms/noodle.rom

echo "Done."

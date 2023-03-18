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
	gcc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${EMU_INC}
	gcc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${CLI_INC}
	cp bin/uxnasm ~/bin
	cp bin/uxncli ~/bin
	cp bin/uxn11 ~/bin
elif [ "${1}" = '--release' ];
then
	gcc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${EMU_INC}
	gcc ${C_FLAGS} ${LD_FLAGS} ${RELEASE_FLAGS} ${CLI_INC}
else
	gcc ${C_FLAGS} ${LD_FLAGS} ${DEBUG_FLAGS} ${EMU_INC}
	gcc ${C_FLAGS} ${LD_FLAGS} ${DEBUG_FLAGS} ${CLI_INC} 
fi

echo "Assembling.."
bin/uxnasm etc/polycat.tal bin/polycat.rom

echo "Running.."
bin/uxn11 bin/polycat.rom

echo "Done."

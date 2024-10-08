
CLI_src=src/uxn.c src/devices/system.c src/devices/console.c src/devices/file.c src/devices/datetime.c
EMU_src=${CLI_src} src/devices/screen.c src/devices/controller.c src/devices/mouse.c

RELEASE_flags=-DNDEBUG -O2 -g0 -s
DEBUG_flags=-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined

PREFIX=${HOME}/.local

.PHONY: all debug dest run test install uninstall format clean grab archive

all: dest bin/uxnasm bin/uxncli bin/uxntui

dest:
	@ mkdir -p bin
run: all bin/uxnasm bin/uxncli bin/uxntui
	@ bin/uxntui
debug: bin/uxnasm-debug bin/uxncli-debug bin/uxntui-debug
	@ bin/uxncli-debug bin/opctest.rom
test: all
	@ bin/uxnasm -v && ./bin/uxncli -v && ./bin/uxntui -v
	@ bin/uxnasm etc/opctest.tal bin/opctest.rom
	@ bin/uxncli bin/opctest.rom
install: all
	@ mkdir -p ${PREFIX}/bin
	@ cp bin/uxntui bin/uxnasm bin/uxncli ${PREFIX}/bin
	@ mkdir -p ${PREFIX}/share/man/man7
	@ cp doc/man/uxntal.7 ${PREFIX}/share/man/man7
uninstall:
	@ rm -f ~/bin/uxntui ~/bin/uxnasm ~/bin/uxncli
format:
	@ clang-format -i src/uxnasm.c src/uxncli.c src/uxntui.c src/devices/*
grab:
	@ cp ../uxn-utils/cli/opctest/opctest.tal etc/opctest.tal
archive:
	@ cp src/uxnasm.c ../oscean/etc/uxnasm.c.txt
clean:
	@ rm -fr bin

bin/uxnasm: src/uxnasm.c
	@ cc ${RELEASE_flags} ${CFLAGS} src/uxnasm.c -o bin/uxnasm
bin/uxncli: ${CLI_src} src/uxncli.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${CLI_src} src/uxncli.c -lutil -o bin/uxncli
bin/uxntui: ${EMU_src} src/uxntui.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${EMU_src} src/uxntui.c -lX11 -lutil -o bin/uxntui

bin/uxnasm-debug: src/uxnasm.c
	@ cc ${DEBUG_flags} ${CFLAGS} src/uxnasm.c -o bin/uxnasm-debug
bin/uxncli-debug: ${CLI_src} src/uxncli.c
	@ cc ${DEBUG_flags} ${CFLAGS} ${CLI_src} src/uxncli.c -lutil -o bin/uxncli-debug
bin/uxntui-debug: ${EMU_src} src/uxntui.c
	@ cc ${DEBUG_flags} ${CFLAGS} ${EMU_src} src/uxntui.c -lX11 -lutil -o bin/uxntui-debug

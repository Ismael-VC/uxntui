
CLI_src=src/uxn.c src/devices/system.c src/devices/console.c src/devices/file.c src/devices/datetime.c
EMU_src=${CLI_src} src/devices/screen.c src/devices/controller.c src/devices/mouse.c

RELEASE_flags=-DNDEBUG -O2 -g0 -s
DEBUG_flags=-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined

.PHONY: all debug dest run test install uninstall format clean archive

all: dest bin/uxnasm bin/uxncli bin/uxn11

dest:
	@ mkdir -p bin
run: all bin/uxnasm bin/uxncli bin/uxn11
	@ bin/uxn11 
debug: bin/uxnasm-debug bin/uxncli-debug bin/uxn11-debug
	@ bin/uxncli-debug bin/opctest.rom
test: all
	@ bin/uxnasm -v && ./bin/uxncli -v && ./bin/uxn11 -v
	@ bin/uxnasm etc/opctest.tal bin/opctest.rom
	@ bin/uxncli bin/opctest.rom
install: all
	@ cp bin/uxn11 bin/uxnasm bin/uxncli ~/bin/
uninstall:
	@ rm -f ~/bin/uxn11 ~/bin/uxnasm ~/bin/uxncli
format:
	@ clang-format -i src/uxnasm.c src/uxncli.c src/uxn11.c src/devices/*
archive:
	@ cp src/uxnasm.c ../oscean/etc/uxnasm.c.txt
clean:
	@ rm -fr bin

bin/uxnasm: src/uxnasm.c
	@ cc ${RELEASE_flags} ${CFLAGS} src/uxnasm.c -o bin/uxnasm
bin/uxncli: ${CLI_src} src/uxncli.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${CLI_src} src/uxncli.c -lutil -o bin/uxncli
bin/uxn11: ${EMU_src} src/uxn11.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${EMU_src} src/uxn11.c -lX11 -lutil -o bin/uxn11
bin/uxnasm-debug: src/uxnasm.c
	@ cc ${DEBUG_flags} ${CFLAGS} src/uxnasm.c -o bin/uxnasm
bin/uxncli-debug: ${CLI_src} src/uxncli.c
	@ cc ${DEBUG_flags} ${CFLAGS} ${CLI_src} src/uxncli.c -lutil -o bin/uxncli
bin/uxn11-debug: ${EMU_src} src/uxn11.c
	@ cc ${DEBUG_flags} ${CFLAGS} ${EMU_src} src/uxn11.c -lX11 -lutil -o bin/uxn11


CLI_src=src/uxn.c src/devices/system.c src/devices/console.c src/devices/file.c src/devices/datetime.c src/devices/link.c
EMU_src=${CLI_src} src/devices/screen.c src/devices/controller.c src/devices/mouse.c

RELEASE_flags=-DNDEBUG -O2 -g0 -s
DEBUG_flags=-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined

.PHONY: all debug dest rom run test install uninstall format clean

all: dest bin/uxnasm bin/uxncli bin/uxn11

dest:
	@ mkdir -p bin
rom:
	@ ./bin/uxnasm etc/screen.bounds.tal bin/screen.bounds.rom
run: all bin/uxnasm bin/uxncli bin/uxn11 rom
	@ ./bin/uxn11 bin/screen.bounds.rom
test: bin/uxnasm bin/uxncli bin/uxn11
	@ ./bin/uxnasm && ./bin/uxncli && ./bin/uxn11 && ./bin/uxnasm -v && ./bin/uxncli -v && ./bin/uxn11 -v
	@ ./bin/uxnasm etc/opctest.tal bin/opctest.rom
	@ ./bin/uxncli bin/opctest.rom
install: bin/uxnasm bin/uxncli bin/uxn11
	@ cp bin/uxn11 bin/uxnasm bin/uxncli ~/bin/
uninstall:
	@ rm -f ~/bin/uxn11 ~/bin/uxnasm ~/bin/uxncli
format:
	@ clang-format -i src/uxnasm.c src/uxncli.c src/uxn11.c src/devices/*
clean:
	@ rm -f bin/uxnasm bin/uxncli bin/uxn11 bin/polycat.rom bin/polycat.rom.sym

bin/uxnasm: src/uxnasm.c
	@ cc ${RELEASE_flags} ${CFLAGS} src/uxnasm.c -o bin/uxnasm
bin/uxncli: ${CLI_src} src/uxncli.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${CLI_src} src/uxncli.c -lutil -pthread -o bin/uxncli
bin/uxn11: ${EMU_src} src/uxn11.c
	@ cc ${RELEASE_flags} ${CFLAGS} ${EMU_src} src/uxn11.c -lX11 -lutil -pthread -o bin/uxn11

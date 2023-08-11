
CLI_src=src/uxn.c src/devices/system.c src/devices/console.c src/devices/file.c src/devices/datetime.c 
EMU_src=${CLI_src} src/devices/screen.c src/devices/controller.c src/devices/mouse.c

RELEASE_flags=-DNDEBUG -O2 -g0 -s
DEBUG_flags=-std=c89 -D_POSIX_C_SOURCE=199309L -DDEBUG -Wall -Wno-unknown-pragmas -Wpedantic -Wshadow -Wextra -Werror=implicit-int -Werror=incompatible-pointer-types -Werror=int-conversion -Wvla -g -Og -fsanitize=address -fsanitize=undefined

all: dest uxnasm uxncli uxn11

uxnasm: 
	cc ${RELEASE_flags} src/uxnasm.c -o bin/uxnasm 
uxncli: 
	gcc ${RELEASE_flags} ${CLI_src} src/uxncli.c -o bin/uxncli
uxn11: 
	gcc ${RELEASE_flags} ${EMU_src} src/uxn11.c -lX11 -o bin/uxn11 

debug: dest uxnasm-debug uxncli-debug uxn11-debug

uxnasm-debug: 
	cc ${DEBUG_flags} src/uxnasm.c -o bin/uxnasm 
uxncli-debug: 
	gcc ${DEBUG_flags} ${CLI_src} src/uxncli.c -o bin/uxncli
uxn11-debug: 
	gcc ${DEBUG_flags} ${EMU_src} src/uxn11.c -lX11 -o bin/uxn11 

dest:
	mkdir -p bin
rom:
	./bin/uxnasm etc/polycat.tal bin/polycat.rom

run: uxnasm uxncli uxn11 rom
	./bin/uxn11 bin/polycat.rom
install: uxnasm uxncli uxn11
	cp bin/uxn11 bin/uxnasm bin/uxncli ~/bin/
uninstall:
	rm -f ~/bin/uxn11 ~/bin/uxnasm ~/bin/uxncli
format:
	clang-format -i src/uxnasm.c src/uxncli.c src/uxn11.c src/devices/*
clean:
	rm -f bin/uxnasm bin/uxncli bin/uxn11 bin/polycat.rom bin/polycat.rom.sym

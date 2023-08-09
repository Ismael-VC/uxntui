# Uxn11

An emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Building 

### Graphical

All you need is X11.

```sh
gcc -Os -DNDEBUG -g0 -s src/uxn.c src/devices/system.c src/devices/console.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -o bin/uxn11 -lX11
```

### Terminal

If you wish to build the emulator without graphics mode:

```sh
gcc -Os -DNDEBUG -g0 -s src/uxn.c src/devices/system.c src/devices/console.c src/devices/file.c src/devices/datetime.c src/uxncli.c -o bin/uxncli
```

If do not wish to build it yourself, you can download linux binaries for [uxnasm](https://rabbits.srht.site/uxn11/bin/uxnasm), [uxncli](https://rabbits.srht.site/uxn11/bin/uxncli) and [uxn11](https://rabbits.srht.site/uxn11/bin/uxn11).

[![builds.sr.ht status](https://builds.sr.ht/~rabbits/uxn11.svg)](https://builds.sr.ht/~rabbits/uxn11?)

## Usage

The first parameter is the rom file, the subsequent arguments will be accessible to the rom, via the [Console vector](https://wiki.xxiivv.com/site/varvara.html#console).

```sh
bin/uxnemu bin/polycat.rom arg1 arg2
```

## Devices

The file device is _sandboxed_, meaning that it should not be able to read or write outside of the working directory.

- `00` system
- `10` console
- `20` screen
- `80` controller
- `90` mouse
- `a0` file
- `c0` datetime

## Emulator Controls

- `F2` toggle on-screen debugger
- `F4` load boot.rom, or reload rom

### Buttons

- `LCTRL` A
- `LALT` B
- `LSHIFT` SEL 
- `HOME` START

## Need a hand?

The following resources are a good place to start:

* [XXIIVV — uxntal](https://wiki.xxiivv.com/site/uxntal.html)
* [XXIIVV — uxntal reference](https://wiki.xxiivv.com/site/uxntal_reference.html)
* [compudanzas — uxn tutorial](https://compudanzas.net/uxn_tutorial.html)

## Contributing

Submit patches using [`git send-email`](https://git-send-email.io/) to the [~rabbits/public-inbox mailing list](https://lists.sr.ht/~rabbits/public-inbox).

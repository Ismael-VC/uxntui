# Uxn11

An emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Building 

### Graphical

All you need is X11.

```sh
gcc -Os -DNDEBUG -g0 -s src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -o bin/uxn11 -lX11
```

### Terminal

If you wish to build the emulator without graphics mode:

```sh
gcc -Os -DNDEBUG -g0 -s src/uxn.c src/devices/system.c src/devices/file.c src/devices/datetime.c src/uxncli.c -o bin/uxncli
```

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
- `30` audio(missing)
- `70` midi(missing)
- `80` controller
- `90` mouse
- `a0` file(sandboxed)
- `c0` datetime

## Emulator Controls

- `F2` print non-empty stacks
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

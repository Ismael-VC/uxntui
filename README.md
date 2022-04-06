# Uxn11

An emulator for the [Uxn stack-machine](https://wiki.xxiivv.com/site/uxn.html), written in ANSI C. 

## Uxn11/System

This emulator's system device supports changing a stack's location to a page of memory. The default memory mapping is as follows:

- `0000-ffff`, as **RAM**.
- `10000-100ff`, as **working stack**.
- `10100-101ff`, as **return stack**.

To use the last page of ram(`0xff00`) to host the working stack:

```
#ff .System/wst DEO
```

The stack mapping is 254 bytes of data, a byte for the pointer and a byte for an error code.

## Building 

### Graphical

All you need is X11.

```sh
gcc src/uxn.c src/devices/system.c src/devices/screen.c src/devices/controller.c src/devices/mouse.c src/devices/file.c src/devices/datetime.c src/uxn11.c -D_POSIX_C_SOURCE=199309L -DNDEBUG -Os -g0 -s -o bin/uxn11 -lX11
```

### Terminal

If you wish to build the emulator without graphics mode:

```sh
cc src/devices/datetime.c src/devices/system.c src/devices/file.c src/uxn.c -DNDEBUG -Os -g0 -s src/uxncli.c -o bin/uxncli
```

## Run

```sh
bin/uxnemu bin/polycat.rom
```

## Devices

- `00` system
- `10` console
- `20` screen
- `30` audio(missing)
- `70` midi(missing)
- `80` controller
- `90` mouse
- `a0` file
- `c0` datetime

## Emulator Controls

- `F2` toggle debug
- `F4` load launcher.rom

### Buttons

- `LCTRL` A
- `LALT` B
- `LSHIFT` SEL 
- `HOME` START

## Need a hand?

The following resources are a good place to start:

* [XXIIVV — uxntal](https://wiki.xxiivv.com/site/uxntal.html)
* [XXIIVV — uxntal cheatsheet](https://wiki.xxiivv.com/site/uxntal_cheatsheet.html)
* [XXIIVV — uxntal reference](https://wiki.xxiivv.com/site/uxntal_reference.html)
* [compudanzas — uxn tutorial](https://compudanzas.net/uxn_tutorial.html)

You can also find us in [`#uxn` on irc.esper.net](ircs://irc.esper.net:6697/#uxn).

## Contributing

Submit patches using [`git send-email`](https://git-send-email.io/) to the [~rabbits/public-inbox mailing list](https://lists.sr.ht/~rabbits/public-inbox).

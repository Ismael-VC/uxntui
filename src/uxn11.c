#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <poll.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/console.h"
#include "devices/screen.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/file.h"
#include "devices/datetime.h"
#include "devices/link.h"

/*
Copyright (c) 2022 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static XImage *ximage;
static Display *display;
static Window window;

#define WIDTH (64 * 8)
#define HEIGHT (40 * 8)
#define PAD 2
#define SCALE 1
#define CONINBUFSIZE 256

static int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

Uint8
emu_dei(Uxn *u, Uint8 addr)
{
	switch(addr & 0xf0) {
	case 0x00: return system_dei(u, addr);
	case 0x20: return screen_dei(u, addr);
	case 0xc0: return datetime_dei(u, addr);
	}
	return u->dev[addr];
}

void
emu_deo(Uxn *u, Uint8 addr, Uint8 value)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	u->dev[addr] = value;
	switch(d) {
	case 0x00:
		system_deo(u, &u->dev[d], p);
		if(p > 0x7 && p < 0xe)
			screen_palette(&u->dev[0x8]);
		break;
	case 0x10: console_deo(&u->dev[d], p); break;
	case 0x20: screen_deo(u->ram, &u->dev[d], p); break;
	case 0xa0: file_deo(0, u->ram, &u->dev[d], p); break;
	case 0xb0: file_deo(1, u->ram, &u->dev[d], p); break;
	case 0xf0: link_deo(u->ram, &u->dev[d], p); break;
	}
}

int
emu_resize(int width, int height)
{
	(void)width;
	(void)height;
	return 1;
}

static void
emu_restart(Uxn *u, char *rom, int soft)
{
	screen_resize(WIDTH, HEIGHT);
	screen_fill(uxn_screen.bg, 0, 0, uxn_screen.width, uxn_screen.height, 0);
	screen_fill(uxn_screen.fg, 0, 0, uxn_screen.width, uxn_screen.height, 0);
	system_reboot(u, rom, soft);
	/* set window title */
}

static int
emu_end(Uxn *u)
{
	free(u->ram);
	XDestroyImage(ximage);
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	exit(0);
	return u->dev[0x0f] & 0x7f;
}

static void
hide_cursor(void)
{
	Cursor blank;
	Pixmap bitmap;
	XColor black;
	static char empty[] = {0};
	black.red = black.green = black.blue = 0;
	bitmap = XCreateBitmapFromData(display, window, empty, 1, 1);
	blank = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
	XDefineCursor(display, window, blank);
	XFreeCursor(display, blank);
	XFreePixmap(display, bitmap);
}

static Uint8
get_button(KeySym sym)
{
	switch(sym) {
	case XK_Up: return 0x10;
	case XK_Down: return 0x20;
	case XK_Left: return 0x40;
	case XK_Right: return 0x80;
	case XK_Control_L: return 0x01;
	case XK_Alt_L: return 0x02;
	case XK_Shift_L: return 0x04;
	case XK_Home: return 0x08;
	}
	return 0x00;
}

static void
emu_event(Uxn *u)
{
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type) {
	case Expose:
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, PAD, PAD, uxn_screen.width, uxn_screen.height);
		break;
	case ClientMessage: {
		emu_end(u);
	} break;
	case KeyPress: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		if(sym == XK_F2)
			u->dev[0x0e] = !u->dev[0x0e];
		else if(sym == XK_F4)
			emu_restart(u, boot_rom, 0);
		else if(sym == XK_F5)
			emu_restart(u, boot_rom, 1);
		controller_down(u, &u->dev[0x80], get_button(sym));
		controller_key(u, &u->dev[0x80], sym < 0x80 ? sym : (Uint8)buf[0]);
	} break;
	case KeyRelease: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		controller_up(u, &u->dev[0x80], get_button(sym));
	} break;
	case ButtonPress: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		if(e->button == 4)
			mouse_scroll(u, &u->dev[0x90], 0, 1);
		else if(e->button == 5)
			mouse_scroll(u, &u->dev[0x90], 0, -1);
		else
			mouse_down(u, &u->dev[0x90], 0x1 << (e->button - 1));
	} break;
	case ButtonRelease: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_up(u, &u->dev[0x90], 0x1 << (e->button - 1));
	} break;
	case MotionNotify: {
		XMotionEvent *e = (XMotionEvent *)&ev;
		int x = clamp((e->x - PAD) / SCALE, 0, uxn_screen.width - 1);
		int y = clamp((e->y - PAD) / SCALE, 0, uxn_screen.height - 1);
		mouse_pos(u, &u->dev[0x90], x, y);
	} break;
	}
}

static int
emu_init(void)
{
	display = XOpenDisplay(NULL);
	if(!display)
		return system_error("X11", "Could not open display");
	screen_resize(WIDTH, HEIGHT);
	return 1;
}

static int
emu_run(Uxn *u, char *rom)
{
	int i = 1, n;
	char expirations[8];
	char coninp[CONINBUFSIZE];
	struct pollfd fds[3];
	static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};

	/* display */
	Atom wmDelete;
	static Visual *visual;
	visual = DefaultVisual(display, 0);
	window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width * SCALE + PAD * 2, uxn_screen.height * SCALE + PAD * 2, 1, 0, 0);
	if(visual->class != TrueColor)
		return system_error("init", "True-color visual failed");
	XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &wmDelete, 1);
	XStoreName(display, window, rom);
	XMapWindow(display, window);
	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width * SCALE, uxn_screen.height * SCALE, 32, 0);
	hide_cursor();

	/* timer */
	fds[0].fd = XConnectionNumber(display);
	fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);
	fds[2].fd = STDIN_FILENO;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;
	/* main loop */
	while(!u->dev[0x0f]) {
		if(poll(fds, 3, 1000) <= 0)
			continue;
		while(XPending(display))
			emu_event(u);
		if(poll(&fds[1], 1, 0)) {
			read(fds[1].fd, expirations, 8);   /* Indicate we handled the timer */
			uxn_eval(u, PEEK2(u->dev + 0x20)); /* Call the vector once, even if the timer fired multiple times */
			if(uxn_screen.x2) {
				int x1 = uxn_screen.x1, y1 = uxn_screen.y1, x2 = uxn_screen.x2, y2 = uxn_screen.y2;
				screen_redraw(u);
				XPutImage(display, window, DefaultGC(display, 0), ximage, x1, y1, x1 + PAD, y1 + PAD, x2 - x1, y2 - y1);
			}
		}
		if((fds[2].revents & POLLIN) != 0) {
			n = read(fds[2].fd, coninp, CONINBUFSIZE - 1);
			coninp[n] = 0;
			for(i = 0; i < n; i++)
				console_input(u, coninp[i], CONSOLE_STD);
		}
	}
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u = {0};
	int i = 1;
	if(i == argc)
		return system_error("usage", "uxn11 [-v] file.rom [args...]");
	/* Read flags */
	if(argv[i][0] == '-' && argv[i][1] == 'v')
		return system_version("Uxn11 - Graphical Varvara Emulator", "31 Oct 2023");
	if(!emu_init())
		return system_error("Init", "Failed to initialize varvara.");
	if(!system_init(&u, (Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8)), argv[i++]))
		return system_error("Init", "Failed to initialize uxn.");
	/* Game Loop */
	u.dev[0x17] = argc - i;
	if(uxn_eval(&u, PAGE_PROGRAM)) {
		console_listen(&u, i, argc, argv);
		emu_run(&u, boot_rom);
	}
	return emu_end(&u);
}

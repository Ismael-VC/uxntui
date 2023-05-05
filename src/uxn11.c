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
#include "devices/screen.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/file.h"
#include "devices/datetime.h"

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
static Visual *visual;
static Window window;

char *rom_path;

#define SCALE 1
#define WIDTH (64 * 8)
#define HEIGHT (40 * 8)
#define PAD 4
#define CONINBUFSIZE 256

Uint16 deo_mask[] = {0xff28, 0x0300, 0xc028, 0x8000, 0x8000, 0x8000, 0x8000, 0x0000, 0x0000, 0x0000, 0xa260, 0xa260, 0x0000, 0x0000, 0x0000, 0x0000};
Uint16 dei_mask[] = {0x0000, 0x0000, 0x003c, 0x0014, 0x0014, 0x0014, 0x0014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x07ff, 0x0000, 0x0000, 0x0000};

Uint8
uxn_dei(Uxn *u, Uint8 addr)
{
	switch(addr & 0xf0) {
	case 0x20: return screen_dei(u, addr);
	case 0xc0: return datetime_dei(u, addr);
	}
	return u->dev[addr];
}

void
uxn_deo(Uxn *u, Uint8 addr)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
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
	}
}

static void
emu_draw(void)
{
	screen_redraw();
	XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, PAD, PAD, uxn_screen.width * SCALE, uxn_screen.height * SCALE);
}

static int
emu_start(Uxn *u, char *rom)
{
	if(!system_load(u, rom))
		return 0;
	if(!uxn_screen.width || !uxn_screen.height)
		screen_resize(WIDTH, HEIGHT);
	if(!uxn_eval(u, PAGE_PROGRAM))
		return system_error("boot", "Failed to start rom.");
	return 1;
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
		emu_draw();
		break;
	case ClientMessage: {
		XDestroyImage(ximage);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		exit(0);
	} break;
	case KeyPress: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		if(sym == XK_F2)
			system_inspect(u);
		if(sym == XK_F4)
			if(!emu_start(u, "boot.rom"))
				emu_start(u, rom_path);
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
		mouse_pos(u, &u->dev[0x90], (e->x - PAD) / SCALE, (e->y - PAD) / SCALE);
	} break;
	}
}

static int
display_start(char *title)
{
	Atom wmDelete;
	display = XOpenDisplay(NULL);
	visual = DefaultVisual(display, 0);
	window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width * SCALE + PAD * 2, uxn_screen.height * SCALE + PAD * 2, 1, 0, 0);
	if(visual->class != TrueColor)
		return system_error("init", "True-color visual failed");
	XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &wmDelete, 1);
	XStoreName(display, window, title);
	XMapWindow(display, window);
	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width * SCALE, uxn_screen.height * SCALE, 32, 0);
	hide_cursor();
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u;
	int i, n;
	char expirations[8];
	char coninp[CONINBUFSIZE];
	struct pollfd fds[3];
	static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};
	if(argc < 2)
		return system_error("usage", "uxn11 game.rom args");
	rom_path = argv[1];
	if(!uxn_boot(&u, (Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8))))
		return system_error("boot", "Failed");
	/* start sequence */
	if(!emu_start(&u, rom_path))
		return system_error("start", rom_path);
	if(!display_start(rom_path))
		return system_error("display", "Failed");
	/* console vector */
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++, CONSOLE_ARG);
		console_input(&u, '\n', i == argc ? CONSOLE_END : CONSOLE_EOA);
	}
	/* timer */
	fds[0].fd = XConnectionNumber(display);
	fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);
	fds[2].fd = STDIN_FILENO;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;
	/* main loop */
	while(!u.dev[0x0f]) {
		if(poll(fds, 3, 1000) <= 0)
			continue;
		while(XPending(display))
			emu_event(&u);
		if(poll(&fds[1], 1, 0)) {
			read(fds[1].fd, expirations, 8);   /* Indicate we handled the timer */
			uxn_eval(&u, PEEK2(&u.dev[0x20])); /* Call the vector once, even if the timer fired multiple times */
		}
		if((fds[2].revents & POLLIN) != 0) {
			n = read(fds[2].fd, coninp, CONINBUFSIZE - 1);
			coninp[n] = 0;
			for(i = 0; i < n; i++)
				console_input(&u, coninp[i], CONSOLE_STD);
		}
		if(uxn_screen.x2)
			emu_draw();
	}
	XDestroyImage(ximage);
	return 0;
}

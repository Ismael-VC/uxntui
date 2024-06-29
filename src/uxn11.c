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

Uxn uxn;

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
#define CONINBUFSIZE 256

static int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

Uint8
emu_dei(Uint8 addr)
{
	switch(addr & 0xf0) {
	case 0x00: return system_dei(addr);
	case 0x10: return console_dei(addr);
	case 0x20: return screen_dei(addr);
	case 0xc0: return datetime_dei(addr);
	}
	return uxn.dev[addr];
}

void
emu_deo(Uint8 addr, Uint8 value)
{
	Uint8 p = addr & 0x0f, d = addr & 0xf0;
	uxn.dev[addr] = value;
	switch(d) {
	case 0x00:
		system_deo(&uxn.dev[d], p);
		if(p > 0x7 && p < 0xe)
			screen_palette(&uxn.dev[0x8]);
		break;
	case 0x10: console_deo(&uxn.dev[d], p); break;
	case 0x20: screen_deo(&uxn.dev[d], p); break;
	case 0xa0: file_deo(0, uxn.ram, &uxn.dev[d], p); break;
	case 0xb0: file_deo(1, uxn.ram, &uxn.dev[d], p); break;
	}
}

int
emu_resize(int w, int h)
{
	if(window) {
		static Visual *visual;
		w *= uxn_screen.scale, h *= uxn_screen.scale;
		visual = DefaultVisual(display, 0);
		ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, w, h, 32, 0);
		XResizeWindow(display, window, w + PAD * 2, h + PAD * 2);
		XMapWindow(display, window);
	}
	return 1;
}

static void
emu_restart(char *rom, int soft)
{
	screen_resize(WIDTH, HEIGHT, uxn_screen.scale);
	screen_rect(uxn_screen.bg, 0, 0, uxn_screen.width, uxn_screen.height, 0);
	screen_rect(uxn_screen.fg, 0, 0, uxn_screen.width, uxn_screen.height, 0);
	system_reboot(rom, soft);
}

static int
emu_end(void)
{
	free(uxn.ram);
	XDestroyImage(ximage);
	XDestroyWindow(display, window);
	XCloseDisplay(display);
	exit(0);
	return uxn.dev[0x0f] & 0x7f;
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
	case XK_Meta_L: return 0x02;
	}
	return 0x00;
}

static void
toggle_scale(void)
{
	int s = uxn_screen.scale + 1;
	if(s > 3) s = 1;
	screen_resize(uxn_screen.width, uxn_screen.height, s);
}

static void
emu_event(void)
{
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type) {
	case Expose: {
		int w = uxn_screen.width * uxn_screen.scale;
		int h = uxn_screen.height * uxn_screen.scale;
		XResizeWindow(display, window, w + PAD * 2, h + PAD * 2);
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, PAD, PAD, w, h);
	} break;
	case ClientMessage:
		uxn.dev[0x0f] = 0x80;
		break;
	case KeyPress: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		switch(sym) {
		case XK_F1: toggle_scale(); break;
		case XK_F2: uxn.dev[0x0e] = !uxn.dev[0x0e]; break;
		case XK_F4: emu_restart(boot_rom, 0); break;
		case XK_F5: emu_restart(boot_rom, 1); break;
		}
		controller_down(&uxn.dev[0x80], get_button(sym));
		controller_key(&uxn.dev[0x80], sym < 0x80 ? sym : (Uint8)buf[0]);
	} break;
	case KeyRelease: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		controller_up(&uxn.dev[0x80], get_button(sym));
	} break;
	case ButtonPress: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		if(e->button == 4)
			mouse_scroll(&uxn.dev[0x90], 0, 1);
		else if(e->button == 5)
			mouse_scroll(&uxn.dev[0x90], 0, -1);
		else if(e->button == 6)
			mouse_scroll(&uxn.dev[0x90], 1, 0);
		else if(e->button == 7)
			mouse_scroll(&uxn.dev[0x90], -1, 0);
		else
			mouse_down(&uxn.dev[0x90], 0x1 << (e->button - 1));
	} break;
	case ButtonRelease: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_up(&uxn.dev[0x90], 0x1 << (e->button - 1));
	} break;
	case MotionNotify: {
		XMotionEvent *e = (XMotionEvent *)&ev;
		int x = clamp((e->x - PAD) / uxn_screen.scale, 0, uxn_screen.width - 1);
		int y = clamp((e->y - PAD) / uxn_screen.scale, 0, uxn_screen.height - 1);
		mouse_pos(&uxn.dev[0x90], x, y);
	} break;
	}
}

static int
display_init(void)
{
	Atom wmDelete;
	static Visual *visual;
	XColor black = {0};
	static char empty[] = {0};
	Pixmap bitmap;
	Cursor blank;
	display = XOpenDisplay(NULL);
	if(!display)
		return system_error("init", "Display failed");
	screen_resize(WIDTH, HEIGHT, 1);
	/* start window */
	visual = DefaultVisual(display, 0);
	if(visual->class != TrueColor)
		return system_error("init", "True-color visual failed");
	window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width + PAD * 2, uxn_screen.height + PAD * 2, 1, 0, 0);
	XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &wmDelete, 1);
	XStoreName(display, window, boot_rom);
	XMapWindow(display, window);
	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width, uxn_screen.height, 32, 0);
	/* hide cursor */
	bitmap = XCreateBitmapFromData(display, window, empty, 1, 1);
	blank = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
	XDefineCursor(display, window, blank);
	XFreeCursor(display, blank);
	XFreePixmap(display, bitmap);
	return 1;
}

static int
emu_run(void)
{
	int i = 1, n;
	char expirations[8];
	char coninp[CONINBUFSIZE];
	struct pollfd fds[3];
	static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};
	/* timer */
	fds[0].fd = XConnectionNumber(display);
	fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);
	fds[2].fd = STDIN_FILENO;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;
	/* main loop */
	while(!uxn.dev[0x0f]) {
		if(poll(fds, 3, 1000) <= 0)
			continue;
		while(XPending(display))
			emu_event();
		if(poll(&fds[1], 1, 0)) {
			read(fds[1].fd, expirations, 8);
			uxn_eval(&uxn, uxn.dev[0x20] << 8 | uxn.dev[0x21]);
			if(screen_changed()) {
				int x = uxn_screen.x1 * uxn_screen.scale, y = uxn_screen.y1 * uxn_screen.scale;
				int w = uxn_screen.x2 * uxn_screen.scale - x, h = uxn_screen.y2 * uxn_screen.scale - y;
				screen_redraw();
				XPutImage(display, window, DefaultGC(display, 0), ximage, x, y, x + PAD, y + PAD, w, h);
			}
		}
		if((fds[2].revents & POLLIN) != 0) {
			n = read(fds[2].fd, coninp, CONINBUFSIZE - 1);
			coninp[n] = 0;
			for(i = 0; i < n; i++)
				console_input(coninp[i], CONSOLE_STD);
		}
	}
	return 1;
}

int
main(int argc, char **argv)
{
	int i = 1;
	char *rom;
	if(i != argc && argv[i][0] == '-' && argv[i][1] == 'v') {
		fprintf(stdout, "Uxn11 - Varvara Emulator, 29 Jun 2024.\n");
		i++;
	}
	rom = i == argc ? "boot.rom" : argv[i++];
	if(!system_boot((Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8)), rom)) {
		fprintf(stdout, "usage: %s [-v] file.rom [args..]\n", argv[0]);
		return 0;
	}
	if(!display_init()) {
		fprintf(stdout, "Could not open display.\n");
		return 0;
	}
	/* Game Loop */
	uxn.dev[0x17] = argc - i;
	if(uxn_eval(&uxn, PAGE_PROGRAM)) {
		console_listen(i, argc, argv);
		emu_run();
	}
	return emu_end();
}

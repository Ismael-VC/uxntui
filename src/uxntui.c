#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>

// #include <X11/Xlib.h>
// #include <X11/Xutil.h>
// #include <X11/keysymdef.h>
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
static struct termios oldt;
static int using_alternate_buffer = 0;



/*
Copyright (c) 2022 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

// static XImage *ximage;
// static Display *display;
// static Window window;

#define WIDTH (64 * 8)
#define HEIGHT (40 * 8)
#define PAD 2
#define CONINBUFSIZE 256
#define CTRL_END 0x01
#define SHIFT_PAGE_DOWN 0x02
#define META_PAGE_UP 0x04

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
	case 0x20: return screen_dei(addr);
	case 0xc0: return datetime_dei(addr);
	}
	return uxn.dev[addr];
}

void
emu_deo(Uint8 addr, Uint8 value)
{
	uxn.dev[addr] = value;
	switch(addr & 0xf0) {
	case 0x00:
		system_deo(addr);
		if(addr > 0x7 && addr < 0xe)
			screen_palette();
		break;
	case 0x10: console_deo(addr); break;
	case 0x20: screen_deo(addr); break;
	case 0xa0: file_deo(addr); break;
	case 0xb0: file_deo(addr); break;
	}
}

int
emu_resize(int w, int h)
{
	// if(window) {
	// 	static Visual *visual;
	// 	w *= uxn_screen.scale, h *= uxn_screen.scale;
	// 	visual = DefaultVisual(display, 0);
	// 	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, w, h, 32, 0);
	// 	XResizeWindow(display, window, w + PAD * 2, h + PAD * 2);
	// 	XMapWindow(display, window);
	// }
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

// Function to restore terminal settings
void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    // Reset non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) & ~O_NONBLOCK);
}


void setup_terminal() {
    struct termios newt;

    // Get the current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);

    // Register the cleanup function to reset the terminal on exit
    atexit(restore_terminal);

    // Copy the original settings to new settings
    newt = oldt;

    // Clear specific input mode flags
    newt.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF);

    // Set output mode flags
    newt.c_oflag &= ~ONLCR; // Disable mapping NL to CR-NL on output

    // Clear specific local mode flags
    newt.c_lflag &= ~(ICANON | ECHO | ISIG); // Disable canonical mode, echo, and signals
    newt.c_lflag &= ~(IUCLC | XCASE | IMAXBEL); // Disable upper-lower case conversion, extended ASCII case conversion, and bell on input line too long

    // Set control mode flags
    newt.c_cflag &= ~(PARENB | PARODD); // Disable parity generation and checking

    // Set control characters
    newt.c_cc[VMIN] = 1; // Minimum number of characters for non-canonical read
    newt.c_cc[VTIME] = 0; // Timeout for non-canonical read (in deciseconds)

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Set non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

static int
emu_end(void)
{
    if (uxn.ram) {
        free(uxn.ram);
        uxn.ram = NULL; // Avoid dangling pointer
    }

    if (uxn_screen.virt_screen) {
        free(uxn_screen.virt_screen);
        uxn_screen.virt_screen = NULL;
    }

    if (uxn_screen.prev_screen) {
        free(uxn_screen.prev_screen);
        uxn_screen.prev_screen = NULL;
    }

    if (uxn_screen.curr_screen) {
        free(uxn_screen.curr_screen);
        uxn_screen.curr_screen = NULL;
    }

    // Restore terminal settings if necessary
    restore_terminal();

	// XDestroyImage(ximage);
	// XDestroyWindow(display, window);
	// XCloseDisplay(display);
	return uxn.dev[0x0f] & 0x7f;
}

static Uint8
get_button(void)
{
    int ch;
    Uint8 button = 0;

    // Read input character
    ch = getchar();

    if (ch == 27) { // ESC sequence
        int next_char = getchar();
        if (next_char == '[') {
            switch (getchar()) {
                case 'A': button = 0x10; break; // Up arrow
                case 'B': button = 0x20; break; // Down arrow
                case 'D': button = 0x40; break; // Left arrow
                case 'C': button = 0x80; break; // Right arrow
                case '5': // Page Up or Meta key
                    if (getchar() == '~') {
                        button = META_PAGE_UP;
                    }
                    break;
                case '6': // Page Down or Shift key
                    if (getchar() == '~') {
                        button = SHIFT_PAGE_DOWN;
                    }
                    break;
                case '4': // End or Ctrl key
                    if (getchar() == '~') {
                        button = CTRL_END;
                    }
                    break;
            }
        } else if (next_char == EOF) {
            // Handle simple ESC keypress for buffer toggle
            if (using_alternate_buffer) {
                printf("\033[?1049l"); // Switch back to the main buffer
                using_alternate_buffer = 0;
            } else {
                printf("\033[?1049h"); // Switch to the alternate buffer
                using_alternate_buffer = 1;
            }
        }
    }

    return button;
}

static void
toggle_scale(void)
{
	// int s = uxn_screen.scale + 1;
	// if(s > 3) s = 1;
	// screen_resize(uxn_screen.width, uxn_screen.height, s);
}

static void
emu_event(void)
{
	// XEvent ev;
	// XNextEvent(display, &ev);
	// switch(ev.type) {
	// case Expose: {
	// 	int w = uxn_screen.width * uxn_screen.scale;
	// 	int h = uxn_screen.height * uxn_screen.scale;
	// 	XResizeWindow(display, window, w + PAD * 2, h + PAD * 2);
	// 	XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, PAD, PAD, w, h);
	// } break;
	// case ClientMessage:
	// 	uxn.dev[0x0f] = 0x80;
	// 	break;
	// case KeyPress: {
	// 	KeySym sym;
	// 	char buf[7];
	// 	XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
	// 	switch(sym) {
	// 	case XK_F1: toggle_scale(); break;
	// 	case XK_F2: uxn.dev[0x0e] = !uxn.dev[0x0e]; break;
	// 	case XK_F3: uxn.dev[0x0f] = 0xff; break;
	// 	case XK_F4: emu_restart(boot_rom, 0); break;
	// 	case XK_F5: emu_restart(boot_rom, 1); break;
	// 	}
	// 	controller_down(get_button(sym));
	// 	controller_key(sym < 0x80 ? sym : (Uint8)buf[0]);
	// } break;
	// case KeyRelease: {
	//	KeySym sym;
	// 	char buf[7];
	// 	XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
	// 	controller_up(get_button(sym));
	// } break;
	// case ButtonPress: {
	// 	XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
	// 	switch(e->button) {
	// 	case 4: mouse_scroll(0, 1); break;
	// 	case 5: mouse_scroll(0, -1); break;
	// 	case 6: mouse_scroll(1, 0); break;
	// 	case 7: mouse_scroll(-1, 0); break;
	// 	default: mouse_down(0x1 << (e->button - 1));
	// 	}
	// } break;
	// case ButtonRelease: {
	// 	XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
	// 	mouse_up(0x1 << (e->button - 1));
	// } break;
	// case MotionNotify: {
	// 	XMotionEvent *e = (XMotionEvent *)&ev;
	// 	int x = clamp((e->x - PAD) / uxn_screen.scale, 0, uxn_screen.width - 1);
	// 	int y = clamp((e->y - PAD) / uxn_screen.scale, 0, uxn_screen.height - 1);
	// 	mouse_pos(x, y);
	// } break;
	// }

    // Call get_button() to fetch the current button state
    Uint8 button = get_button();

    // If a button was pressed, handle it
    if (button) {
        switch (button) {
            case 0x10: // Up arrow
                // Handle up arrow press
                controller_down(button);
                // Use specific key codes if necessary
                controller_key(0x10);
                break;
            case 0x20: // Down arrow
                // Handle down arrow press
                controller_down(button);
                controller_key(0x20);
                break;
            case 0x40: // Left arrow
                // Handle left arrow press
                controller_down(button);
                controller_key(0x40);
                break;
            case 0x80: // Right arrow
                // Handle right arrow press
                controller_down(button);
                controller_key(0x80);
                break;
            case CTRL_END:
                // Handle Ctrl+End or similar key
                controller_down(button);
                controller_key(CTRL_END);
                break;
            case SHIFT_PAGE_DOWN:
                // Handle Shift+Page Down or similar key
                controller_down(button);
                controller_key(SHIFT_PAGE_DOWN);
                break;
            case META_PAGE_UP:
                // Handle Meta+Page Up or similar key
                controller_down(button);
                controller_key(META_PAGE_UP);
                break;
            default:
                // Handle other keys or sequences
                break;
        }
    }
}

static int display_init(void) {
    // Atom wmDelete;
    // Visual *visual;
    // XColor black = {0};
    // char empty[] = {0};
    // Pixmap bitmap;
    // Cursor blank;
    // display = XOpenDisplay(NULL);
    // if (!display)
    //     return system_error("init", "Display failed");

	setup_terminal();

    screen_resize(WIDTH, HEIGHT, 1);

    // Allocate memory for screen buffers
    Uint16 w = uxn_screen.width, h = uxn_screen.height;
    uxn_screen.virt_screen = malloc(w * h * sizeof(Uint32));
    uxn_screen.prev_screen = malloc(w * h * sizeof(Uint32));
    uxn_screen.curr_screen = malloc(w * h * sizeof(Uint32));

    if (!uxn_screen.virt_screen || !uxn_screen.prev_screen || !uxn_screen.curr_screen) {
        perror("Failed to allocate memory for screen buffers");
        return 0; // Indicate failure
    }

    // Initialize screen buffers
    memset(uxn_screen.virt_screen, 0, w * h * sizeof(Uint32));
    memset(uxn_screen.prev_screen, 0, w * h * sizeof(Uint32));
    memset(uxn_screen.curr_screen, 0, w * h * sizeof(Uint32));

    // // Start window
    // visual = DefaultVisual(display, 0);
    // if (visual->class != TrueColor)
    //     return system_error("init", "True-color visual failed");

    // window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width + PAD * 2, uxn_screen.height + PAD * 2, 1, 0, 0);
    // XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
    // wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
    // XSetWMProtocols(display, window, &wmDelete, 1);
    // XStoreName(display, window, boot_rom);
    // XMapWindow(display, window);

    // ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width, uxn_screen.height, 32, 0);

    // // Hide cursor
    // bitmap = XCreateBitmapFromData(display, window, empty, 1, 1);
    // blank = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
    // XDefineCursor(display, window, blank);
    // XFreeCursor(display, blank);
    // XFreePixmap(display, bitmap);

    return 1; // Indicate success
}

static int
emu_run(void)
{
    int i = 1, n;
    char expirations[8], coninp[CONINBUFSIZE];
    struct pollfd fds[3];
    static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};

    // Timer file descriptor
    fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
    timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);

    // Standard input file descriptor
    fds[2].fd = STDIN_FILENO;

    // Set the events we are interested in
    fds[1].events = fds[2].events = POLLIN;

    /* main loop */
    while (!uxn.dev[0x0f]) {
    	emu_event();
        if (poll(fds, 1, 0) > 0)
            // emu_event(); // Call emu_event() if there's input

        // Poll for input events (timer or stdin)
        if (poll(fds + 1, 2, 1000) <= 0) // Only poll fds[1] and fds[2]
            continue;

        // Handle timer events
        if (fds[1].revents & POLLIN) {
            read(fds[1].fd, expirations, 8);
            uxn_eval(uxn.dev[0x20] << 8 | uxn.dev[0x21]);
            if (screen_changed()) {
                int x = uxn_screen.x1 * uxn_screen.scale;
                int y = uxn_screen.y1 * uxn_screen.scale;
                int w = uxn_screen.x2 * uxn_screen.scale - x;
                int h = uxn_screen.y2 * uxn_screen.scale - y;
                screen_redraw();
                // XPutImage(display, window, DefaultGC(display, 0), ximage, x, y, x + PAD, y + PAD, w, h);
            }
        }

        // Handle standard input events
        if (fds[2].revents & POLLIN) {
            n = read(fds[2].fd, coninp, CONINBUFSIZE - 1);
            coninp[n] = 0;
            for (i = 0; i < n; i++) {
                console_input(coninp[i], CONSOLE_STD);
            }
            // emu_event(); // Call emu_event() to handle input
        }
    }
    return 1;
}

int
main(int argc, char **argv)
{
	fprintf(stdout, "\e[2J");
	fflush(stdout);
	int i = 1;
	char *rom;
	if(i != argc && argv[i][0] == '-' && argv[i][1] == 'v') {
		fprintf(stdout, "UxnTUI - Varvara Emulator, 23 Aug 2024.\n");
		exit(0);
	}
	rom = i == argc ? "boot.rom" : argv[i++];
	if(!system_boot((Uint8 *)calloc(0x10000 * RAM_PAGES, sizeof(Uint8)), rom))
		return !fprintf(stdout, "usage: %s [-v] file.rom [args..]\n", argv[0]);
	if(!display_init())
		return !fprintf(stdout, "Could not open display.\n");
	/* Event Loop */
	uxn.dev[0x17] = argc - i;
	if(uxn_eval(PAGE_PROGRAM))
		console_listen(i, argc, argv), emu_run();
	return emu_end();
}

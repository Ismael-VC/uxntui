#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux
#include <pty.h>
#endif

#ifdef __NetBSD__
#include <sys/ioctl.h>
#include <util.h>
#endif

#include "../uxn.h"
#include "console.h"

/*
Copyright (c) 2022-2023 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* process */
static char *fork_args[32];
static pid_t child_pid;
static int child_mode;
static int pty_fd;
static int to_child_fd[2];
static int from_child_fd[2];
static int saved_in;
static int saved_out;

struct winsize ws = {24, 80, 8, 12};

static void
parse_args(Uxn *u, Uint8 *d)
{
	Uint8 *port_addr = d + 0x3;
	int addr = PEEK2(port_addr);
	char *pos = (char *)&u->ram[addr];
	int i = 0;
	do {
		fork_args[i++] = pos;
		while(*pos != 0) pos++;
		pos++;
	} while(*pos != '\0');
	fork_args[i] = NULL;
}

/* call after we're sure the process has exited */
static void
clean_after_child(void)
{
	child_pid = 0;
	if(child_mode >= 0x80) {
		close(pty_fd);
		dup2(saved_in, 0);
		dup2(saved_out, 1);
	} else {
		if(child_mode & 0x01) {
			close(to_child_fd[1]);
			dup2(saved_out, 1);
		}
		if(child_mode & 0x06) {
			close(from_child_fd[0]);
			dup2(saved_in, 0);
		}
	}
	child_mode = 0;
	saved_in = -1;
	saved_out = -1;
}

int
console_input(Uxn *u, char c, int type)
{
	Uint8 *d = &u->dev[0x10];
	d[0x2] = c;
	d[0x7] = type;
	return uxn_eval(u, PEEK2(d));
}

void
console_listen(Uxn *u, int i, int argc, char **argv)
{
	for(; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(u, *p++, CONSOLE_ARG);
		console_input(u, '\n', i == argc - 1 ? CONSOLE_END : CONSOLE_EOA);
	}
}

static void
start_fork_pty(Uint8 *d)
{
	int fd = -1;
	pid_t pid = forkpty(&fd, NULL, NULL, NULL);
	if(pid < 0) { /* failure */
		d[0x6] = 0xff;
		fprintf(stderr, "fork failure\n");
	} else if(pid == 0) { /* child */
		setenv("TERM", "ansi", 1);
		execvp(fork_args[0], fork_args);
		d[0x6] = 0xff;
		fprintf(stderr, "exec failure\n");
	} else { /*parent*/
		child_pid = pid;
		pty_fd = fd;
		ioctl(fd, TIOCSWINSZ, &ws);
		saved_in = dup(0);
		saved_out = dup(1);
		dup2(fd, 0);
		dup2(fd, 1);
	}
}

static void
start_fork_pipe(Uint8 *d)
{
	if(child_mode & 0x01) {
		/* parent writes to child's stdin */
		if(pipe(to_child_fd) == -1) {
			d[0x6] = 0xff;
			fprintf(stderr, "pipe error: to child\n");
			return;
		}
	}

	if(child_mode & 0x06) {
		/* parent reads from child's stdout and/or stderr */
		if(pipe(from_child_fd) == -1) {
			d[0x6] = 0xff;
			fprintf(stderr, "pipe error: from child\n");
			return;
		}
	}

	pid_t pid = fork();
	if(pid < 0) { /* failure */
		d[0x6] = 0xff;
		fprintf(stderr, "fork failure\n");
	} else if(pid == 0) { /* child */
		if(child_mode & 0x01) {
			dup2(to_child_fd[0], 0);
			close(to_child_fd[1]);
		}
		if(child_mode & 0x06) {
			if(child_mode & 0x02) dup2(from_child_fd[1], 1);
			if(child_mode & 0x04) dup2(from_child_fd[1], 2);
			close(from_child_fd[0]);
		}
		execvp(fork_args[0], fork_args);
		d[0x6] = 0xff;
		fprintf(stderr, "exec failure\n");
	} else { /*parent*/
		child_pid = pid;
		if(child_mode & 0x01) {
			saved_out = dup(1);
			dup2(to_child_fd[1], 1);
			close(to_child_fd[0]);
		}
		if(child_mode & 0x06) {
			saved_in = dup(0);
			dup2(from_child_fd[0], 0);
			close(from_child_fd[1]);
		}
	}
}

static void
kill_child(Uint8 *d)
{
	if(child_pid) {
		kill(child_pid, 9);
		int wstatus;
		if(waitpid(child_pid, &wstatus, 0)) {
			d[0x6] = 1;
			d[0x7] = WEXITSTATUS(wstatus);
			clean_after_child();
		}
	}
}

static void
start_fork(Uxn *u, Uint8 *d)
{
	fflush(stderr);
	kill_child(d);
	child_mode = d[0x5];
	parse_args(u, d);
	if(child_mode >= 0x80)
		start_fork_pty(d);
	else
		start_fork_pipe(d);
}

Uint8
console_dei(Uxn *u, Uint8 addr)
{
	Uint8 port = addr & 0x0f, *d = &u->dev[addr & 0xf0];
	switch(port) {
	case 0x6:
	case 0x7:
		if(child_pid) {
			int wstatus;
			if(waitpid(child_pid, &wstatus, WNOHANG)) {
				d[0x6] = 1;
				d[0x7] = WEXITSTATUS(wstatus);
				clean_after_child();
			}
		}
	}
	return d[port];
}

void
console_deo(Uxn *u, Uint8 *d, Uint8 port)
{
	FILE *fd = NULL;
	switch(port) {
	case 0x5: /* Console/dead */ start_fork(u, d); break;
	case 0x6: /* Console/exit*/ kill_child(d); break;
	case 0x8: fd = stdout; break;
	case 0x9: fd = stderr; break;
	}
	if(fd) {
		fputc(d[port], fd);
		fflush(fd);
	}
}

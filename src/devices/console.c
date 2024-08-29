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
#include <sys/prctl.h>
#endif

#ifdef __NetBSD__
#include <sys/ioctl.h>
#include <util.h>
#endif

#include "../uxn.h"
#include "console.h"

/*
Copyright (c) 2022-2024 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

/* subprocess support */
static char *fork_args[4] = {"/bin/sh", "-c", "", NULL};
static int child_mode;
static int to_child_fd[2];
static int from_child_fd[2];
static int saved_in;
static int saved_out;
static pid_t child_pid;

/* child_mode:
 * 0x01: writes to child's stdin
 * 0x02: reads from child's stdout
 * 0x04: reads from child's stderr
 * 0x08: kill previous process (if any) but do not start
 * (other bits ignored for now )
 */

#define CMD_LIVE 0x15 /* 0x00 not started, 0x01 running, 0xff dead */
#define CMD_EXIT 0x16 /* if dead, exit code of process */
#define CMD_ADDR 0x1c /* address to read command args from */
#define CMD_MODE 0x1e /* mode to execute, 0x00 to 0x07 */
#define CMD_EXEC 0x1f /* write to execute programs, etc */

/* call after we're sure the process has exited */
static void
clean_after_child(void)
{
	child_pid = 0;
	if(child_mode & 0x01) {
		close(to_child_fd[1]);
		dup2(saved_out, 1);
	}
	if(child_mode & (0x04 | 0x02)) {
		close(from_child_fd[0]);
		dup2(saved_in, 0);
	}
	child_mode = 0;
	saved_in = -1;
	saved_out = -1;
}

static void
start_fork_pipe(void)
{
	pid_t pid;
	pid_t parent_pid = getpid();
	int addr = PEEK2(&uxn.dev[CMD_ADDR]);
	fflush(stdout);
	if(child_mode & 0x08) {
		uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0x00;
		return;
	}
	if(child_mode & 0x01) {
		/* parent writes to child's stdin */
		if(pipe(to_child_fd) == -1) {
			uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
			fprintf(stderr, "pipe error: to child\n");
			return;
		}
	}
	if(child_mode & (0x04 | 0x02)) {
		/* parent reads from child's stdout and/or stderr */
		if(pipe(from_child_fd) == -1) {
			uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
			fprintf(stderr, "pipe error: from child\n");
			return;
		}
	}

	fork_args[2] = (char *)&uxn.ram[addr];
	pid = fork();
	if(pid < 0) { /* failure */
		uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
		fprintf(stderr, "fork failure\n");
	} else if(pid == 0) { /* child */

#ifdef __linux__
		int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
		if(r == -1) {
			perror(0);
			exit(6);
		}
		if(getppid() != parent_pid) exit(13);
#endif

		if(child_mode & 0x01) {
			dup2(to_child_fd[0], 0);
			close(to_child_fd[1]);
		}
		if(child_mode & (0x04 | 0x02)) {
			if(child_mode & 0x02) dup2(from_child_fd[1], 1);
			if(child_mode & 0x04) dup2(from_child_fd[1], 2);
			close(from_child_fd[0]);
		}
		fflush(stdout);
		execvp(fork_args[0], fork_args);
		exit(1);
	} else { /*parent*/
		child_pid = pid;
		uxn.dev[CMD_LIVE] = 0x01;
		uxn.dev[CMD_EXIT] = 0x00;
		if(child_mode & 0x01) {
			saved_out = dup(1);
			dup2(to_child_fd[1], 1);
			close(to_child_fd[0]);
		}
		if(child_mode & (0x04 | 0x02)) {
			saved_in = dup(0);
			dup2(from_child_fd[0], 0);
			close(from_child_fd[1]);
		}
	}
}

static void
check_child(void)
{
	int wstatus;
	if(child_pid) {
		if(waitpid(child_pid, &wstatus, WNOHANG)) {
			uxn.dev[CMD_LIVE] = 0xff;
			uxn.dev[CMD_EXIT] = WEXITSTATUS(wstatus);
			clean_after_child();
		} else {
			uxn.dev[CMD_LIVE] = 0x01;
			uxn.dev[CMD_EXIT] = 0x00;
		}
	}
}

static void
kill_child(void)
{
	int wstatus;
	if(child_pid) {
		kill(child_pid, 9);
		if(waitpid(child_pid, &wstatus, WNOHANG)) {
			uxn.dev[CMD_LIVE] = 0xff;
			uxn.dev[CMD_EXIT] = WEXITSTATUS(wstatus);
			clean_after_child();
		}
	}
}

static void
start_fork(void)
{
	fflush(stderr);
	kill_child();
	child_mode = uxn.dev[CMD_MODE];
	start_fork_pipe();
}

void
close_console(void)
{
	kill_child();
}

int
console_input(Uint8 c, int type)
{
	uxn.dev[0x12] = c;
	uxn.dev[0x17] = type;
	return uxn_eval(PEEK2(&uxn.dev[0x10]));
}

void
console_listen(int i, int argc, char **argv)
{
	for(; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(*p++, CONSOLE_ARG);
		console_input('\n', i == argc - 1 ? CONSOLE_END : CONSOLE_EOA);
	}
}

Uint8
console_dei(Uint8 addr)
{
	switch(addr) {
	case CMD_LIVE:
	case CMD_EXIT: check_child(); break;
	}
	return uxn.dev[addr];
}

void
console_deo(Uint8 addr)
{
	FILE *fd;
	switch(addr) {
	case 0x18: fd = stdout, fputc(uxn.dev[0x18], fd), fflush(fd); break;
	case 0x19: fd = stderr, fputc(uxn.dev[0x19], fd), fflush(fd); break;
	case CMD_EXEC: start_fork(); break;
	}
}

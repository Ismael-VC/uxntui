#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef _WIN32
#include <sys/select.h>
#include <sys/wait.h>
#endif

#ifdef __linux
#include <pty.h>
#include <sys/prctl.h>
#endif

#ifdef __NetBSD__
#include <sys/ioctl.h>
#include <util.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <windows.h>
#include <process.h>
#include <string.h>
#include <direct.h> // For _fullpath

static HANDLE to_child_read;
static HANDLE to_child_write;
static HANDLE from_child_read;
static HANDLE from_child_write;
static HANDLE child_process;
static HANDLE child_stdin;
static HANDLE child_stdout;
static HANDLE child_stderr;

// Define lrealpath as an alias to realpath on Windows
#define lrealpath(path, resolved) realpath(path, resolved)

// On Windows, _fullpath can be used as a substitute for realpath
char *realpath(const char *path, char *resolved) {
    return _fullpath(resolved, path, _MAX_PATH);
}
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
#ifndef _WIN32
static char *fork_args[4] = {"/bin/sh", "-c", "", NULL};
#else
static char *fork_args[4] = {"cmd.exe", "/c", "", NULL};
#endif
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

static void start_fork_pipe(void) {
    int addr = PEEK2(&uxn.dev[CMD_ADDR]);
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&to_child_read, &to_child_write, &sa, 0)) {
        fprintf(stderr, "pipe error: to child\n");
        uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
        return;
    }

    if (!CreatePipe(&from_child_read, &from_child_write, &sa, 0)) {
        fprintf(stderr, "pipe error: from child\n");
        uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
        return;
    }

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    BOOL success;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = to_child_read;
    si.hStdOutput = from_child_write;
    si.hStdError = from_child_write;
    si.dwFlags |= STARTF_USESTDHANDLES;

    fork_args[2] = (char *)&uxn.ram[addr];

    char args[1024];
    snprintf(args, sizeof(args), "%s %s %s", fork_args[0], fork_args[1], fork_args[2]);

    printf("arg: %s\n", args);  // Ensure the command line looks correct

    success = CreateProcess(
        NULL,
        args,          // Command line to execute
        NULL,
        NULL,
        TRUE,          // Set to TRUE to inherit handles
        0,
        NULL,
        NULL,
        &si,
        &pi
    );

    if (!success) {
        printf("CreateProcess failed (%d).\n", GetLastError());
        uxn.dev[CMD_EXIT] = uxn.dev[CMD_LIVE] = 0xff;
        return;
    }

    // Wait for the child process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Read output from child process
    DWORD bytesRead;
    CHAR buffer[4096];

    CloseHandle(from_child_write);  // Close the write end to receive EOF when the process exits

    while (ReadFile(from_child_read, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        printf("%s", buffer);  // Output child's stdout to console
    }

    child_process = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(to_child_read);
    CloseHandle(from_child_write);

#else
    // POSIX implementation (not shown)
#endif
}


static void
check_child(void)
{
#ifdef _WIN32
    DWORD exitCode;
    if (GetExitCodeProcess(child_process, &exitCode) && exitCode != STILL_ACTIVE) {
        uxn.dev[CMD_LIVE] = 0xff;
        uxn.dev[CMD_EXIT] = exitCode;
        clean_after_child();
    } else {
        uxn.dev[CMD_LIVE] = 0x01;
        uxn.dev[CMD_EXIT] = 0x00;
    }
#else
    int wstatus;
    if (waitpid(child_pid, &wstatus, WNOHANG)) {
        uxn.dev[CMD_LIVE] = 0xff;
        uxn.dev[CMD_EXIT] = WEXITSTATUS(wstatus);
        clean_after_child();
    } else {
        uxn.dev[CMD_LIVE] = 0x01;
        uxn.dev[CMD_EXIT] = 0x00;
    }
#endif
}


static void
kill_child(void)
{
#ifdef _WIN32
    if (child_process) {
        TerminateProcess(child_process, 1);
        CloseHandle(child_process);
        check_child();
    }
#else
    if (child_pid) {
        kill(child_pid, 9);
        check_child();
    }
#endif
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


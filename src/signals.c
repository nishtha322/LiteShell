/*
 * signals.c
 * Handles Ctrl+C and background processes.
 */

#include "signals.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t g_sigint_flag = 0;

/* Set a flag when Ctrl+C is pressed. */
static void sigint_handler(int signo)
{
    (void)signo;
    g_sigint_flag = 1;
}

/* Print a string safely inside a signal handler. */
static void async_write_str(const char *s)
{
    write(STDOUT_FILENO, s, strlen(s));
}

/* Print an integer safely inside a signal handler. */
static void async_write_int(long value)
{
    char buf[32];
    int i = (int)sizeof(buf);
    buf[--i] = '\0';

    if (value == 0) {
        buf[--i] = '0';
    } else {
        long v = value;
        int negative = v < 0;

        if (negative) {
            v = -v;
        }

        while (v > 0 && i > 0) {
            buf[--i] = (char)('0' + (v % 10));
            v /= 10;
        }

        if (negative && i > 0) {
            buf[--i] = '-';
        }
    }

    write(STDOUT_FILENO, buf + i, strlen(buf + i));
}

/* Clean up finished background processes. */
static void sigchld_handler(int signo)
{
    (void)signo;

    int status;
    pid_t pid;

    /* Save errno before calling waitpid(). */
    int saved_errno = errno;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        async_write_str("\n[background job ");
        async_write_int((long)pid);
        async_write_str("] done\n");
    }

    errno = saved_errno;
}

void signals_init(void)
{
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);

    /* Let read() return when Ctrl+C is pressed. */
    sa_int.sa_flags = 0;

    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);

    /* Don't interrupt foreground waitpid(). */
    sa_chld.sa_flags = SA_RESTART;

    sigaction(SIGCHLD, &sa_chld, NULL);
}
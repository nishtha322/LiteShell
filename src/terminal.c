/*
 * terminal.c
 * Handles terminal input and basic line editing.
 */

#include "terminal.h"
#include "signals.h"
#include "history.h"
#include "utils.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

/* Original terminal settings. */
static struct termios g_original_termios;
static int g_have_saved_state = 0;
static int g_raw_mode_active = 0;

/* Enable raw mode for character-by-character input. */
static void enable_raw_mode(void)
{
    if (g_raw_mode_active) {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &g_original_termios) == -1) {
        /* Input is not from a terminal. */
        return;
    }

    g_have_saved_state = 1;

    struct termios raw = g_original_termios;

    /* Disable line buffering and echo. */
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);

    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    g_raw_mode_active = 1;
}

void terminal_restore(void)
{
    if (g_raw_mode_active && g_have_saved_state) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_original_termios);
    }

    g_raw_mode_active = 0;
}

/* Redraw the current input line. */
static void redraw_line(const char *prompt, const char *buffer, size_t prev_len)
{
    printf("\r%s", prompt);
    printf("%s", buffer);

    size_t cur_len = strlen(buffer);

    /* Clear leftover characters from the previous line. */
    if (prev_len > cur_len) {
        for (size_t i = 0; i < prev_len - cur_len; i++) {
            putchar(' ');
        }

        for (size_t i = 0; i < prev_len - cur_len; i++) {
            putchar('\b');
        }
    }

    fflush(stdout);
}

char *terminal_read_line(void)
{
    enable_raw_mode();

    size_t capacity = 256;
    size_t len = 0;

    char *buffer = xmalloc(capacity);
    buffer[0] = '\0';

    /* -1 means no history entry is selected. */
    int history_index = -1;

    for (;;) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n < 0) {
            if (errno == EINTR) {

                /* Ctrl+C pressed. */
                if (g_sigint_flag) {
                    g_sigint_flag = 0;
                    printf("\n");
                    fflush(stdout);
                    terminal_restore();
                    free(buffer);
                    return xstrdup("");
                }

                continue;
            }

            /* Input error. */
            terminal_restore();
            free(buffer);
            return xstrdup("");
        }

        if (n == 0) {
            /* End of input. */
            terminal_restore();

            if (len == 0) {
                free(buffer);
                return NULL;
            }

            buffer[len] = '\0';
            return buffer;
        }

        /* Ctrl+D exits on an empty line. */
        if (c == 4 && len == 0) {
            terminal_restore();
            free(buffer);
            return NULL;
        }

        /* Enter key. */
        if (c == '\r' || c == '\n') {
            printf("\n");
            fflush(stdout);
            buffer[len] = '\0';
            terminal_restore();
            return buffer;
        }

        /* Backspace. */
        if (c == 127 || c == '\b') {
            if (len > 0) {
                len--;
                buffer[len] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        /* Arrow keys. */
        if (c == 27) {
            char seq[2];

            if (read(STDIN_FILENO, &seq[0], 1) != 1)
                continue;

            if (seq[0] != '[')
                continue;

            if (read(STDIN_FILENO, &seq[1], 1) != 1)
                continue;

            if (seq[1] == 'A') {
                /* Up arrow: previous command. */
                if (history_index + 1 < history_count()) {
                    history_index++;

                    const char *entry = history_get(history_index);
                    size_t prev_len = len;

                    if (entry != NULL) {
                        strncpy(buffer, entry, capacity - 1);
                        buffer[capacity - 1] = '\0';
                        len = strlen(buffer);
                    }

                    redraw_line("", buffer,
                                prev_len > len ? prev_len : len);
                }

            } else if (seq[1] == 'B') {
                /* Down arrow: next command. */
                size_t prev_len = len;

                if (history_index > 0) {
                    history_index--;

                    const char *entry = history_get(history_index);

                    if (entry != NULL) {
                        strncpy(buffer, entry, capacity - 1);
                        buffer[capacity - 1] = '\0';
                        len = strlen(buffer);
                    }
                } else {
                    history_index = -1;
                    buffer[0] = '\0';
                    len = 0;
                }

                redraw_line("", buffer,
                            prev_len > len ? prev_len : len);
            }

            continue;
        }

        /* Ignore other control characters. */
        if ((unsigned char)c < 32) {
            continue;
        }

        /* Add a normal character. */
        if (len + 1 >= capacity) {
            capacity *= 2;
            buffer = xrealloc(buffer, capacity);
        }

        buffer[len++] = c;
        buffer[len] = '\0';

        putchar(c);
        fflush(stdout);
    }
}
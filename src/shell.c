/*
 * shell.c
 * Main loop of LiteShell.
 */

#include "shell.h"
#include "terminal.h"
#include "history.h"
#include "parser.h"
#include "executor.h"
#include "environment.h"
#include "signals.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>

ShellState g_shell_state = {
    .last_exit_status = 0,
    .running = 1,
    .shell_pid = 0
};

/* Build the shell prompt. */
static void build_prompt(char *out, size_t out_size)
{
    char username[256];
    char hostname[256];
    char cwd[PATH_MAX];

    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL) {
        snprintf(username, sizeof(username), "%s", pw->pw_name);
    } else {
        snprintf(username, sizeof(username), "user");
    }

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        snprintf(hostname, sizeof(hostname), "unknown");
    }

    const char *display_cwd = "?";
    char shortened[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        const char *home = env_get("HOME");

        /* Show home directory as '~'. */
        if (home != NULL && strncmp(cwd, home, strlen(home)) == 0) {
            size_t home_len = strlen(home);

            if (cwd[home_len] == '\0') {
                snprintf(shortened, sizeof(shortened), "~");
            } else if (cwd[home_len] == '/') {
                snprintf(shortened, sizeof(shortened), "~%s", cwd + home_len);
            } else {
                snprintf(shortened, sizeof(shortened), "%s", cwd);
            }
        } else {
            snprintf(shortened, sizeof(shortened), "%s", cwd);
        }

        display_cwd = shortened;
    }

    snprintf(out, out_size, "%s@%s:%s$ ",
             username, hostname, display_cwd);
}

int main(void)
{
    g_shell_state.shell_pid = getpid();

    env_init();
    history_init();
    signals_init();

    while (g_shell_state.running) {
        char prompt[PATH_MAX + 512];

        build_prompt(prompt, sizeof(prompt));
        fputs(prompt, stdout);
        fflush(stdout);

        char *line = terminal_read_line();

        if (line == NULL) {
            /* Exit on Ctrl+D. */
            printf("exit\n");
            break;
        }

        char *trimmed = trim_whitespace(line);

        /* Ignore empty input. */
        if (is_blank_string(trimmed)) {
            free(line);
            continue;
        }

        history_add(trimmed);

        Pipeline pipeline;

        if (parse_line(trimmed, &pipeline) != 0) {
            /* Skip invalid commands. */
            free(line);
            continue;
        }

        execute_pipeline(&pipeline);
        free_pipeline(&pipeline);
        free(line);
    }

    history_cleanup();
    env_cleanup();
    terminal_restore();

    return g_shell_state.last_exit_status;
}
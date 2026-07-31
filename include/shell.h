/*
 * shell.h
 * Shared definitions for LiteShell.
 */

#ifndef LITESHELL_SHELL_H
#define LITESHELL_SHELL_H

#include <sys/types.h>

/* Configuration */
#define MAX_LINE_LENGTH       4096
#define MAX_ARGS               128
#define MAX_PIPELINE_STAGES     32
#define MAX_HISTORY            100
#define MAX_HISTORY_LINE      1024

#define SHELL_NAME "LiteShell"

#define SHELL_EXIT_SUCCESS 0
#define SHELL_EXIT_FAILURE 1

/* A single command. */
typedef struct {
    char *argv[MAX_ARGS + 1];
    int argc;
    char *input_file;
    char *output_file;
    int append_mode;
} Command;

/* A parsed command line. */
typedef struct {
    Command commands[MAX_PIPELINE_STAGES];
    int num_commands;
    int background;
} Pipeline;

/* Shell state. */
typedef struct {
    int last_exit_status;
    int running;
    pid_t shell_pid;
} ShellState;

/* Global shell state. */
extern ShellState g_shell_state;

#endif /* LITESHELL_SHELL_H */
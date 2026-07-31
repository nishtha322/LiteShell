/*
 * builtins.c
 * Implements LiteShell built-in commands.
 */

#include "builtins.h"
#include "environment.h"
#include "history.h"
#include "utils.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>

/* Environment variables. */
extern char **environ;

typedef int (*BuiltinFunc)(char **argv, int argc);

static int builtin_cd(char **argv, int argc);
static int builtin_pwd(char **argv, int argc);
static int builtin_history(char **argv, int argc);
static int builtin_help(char **argv, int argc);
static int builtin_clear(char **argv, int argc);
static int builtin_exit(char **argv, int argc);
static int builtin_echo(char **argv, int argc);
static int builtin_export(char **argv, int argc);
static int builtin_unset(char **argv, int argc);

typedef struct {
    const char *name;
    BuiltinFunc func;
} BuiltinEntry;

static const BuiltinEntry builtin_table[] = {
    { "cd",      builtin_cd      },
    { "pwd",     builtin_pwd     },
    { "history", builtin_history },
    { "help",    builtin_help    },
    { "clear",   builtin_clear   },
    { "exit",    builtin_exit    },
    { "echo",    builtin_echo    },
    { "export",  builtin_export  },
    { "unset",   builtin_unset   },
};

static const int builtin_table_size =
    (int)(sizeof(builtin_table) / sizeof(builtin_table[0]));

int is_builtin(const char *name)
{
    for (int i = 0; i < builtin_table_size; i++) {
        if (strcmp(builtin_table[i].name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

int execute_builtin(char **argv, int argc)
{
    for (int i = 0; i < builtin_table_size; i++) {
        if (strcmp(builtin_table[i].name, argv[0]) == 0) {
            return builtin_table[i].func(argv, argc);
        }
    }

    /* Should never happen. */
    print_message("internal error: execute_builtin called on non-builtin");
    return SHELL_EXIT_FAILURE;
}

/* Change the current directory. */
static int builtin_cd(char **argv, int argc)
{
    const char *target;

    if (argc < 2) {
        target = env_get("HOME");

        if (target == NULL) {
            print_message("cd: HOME not set");
            return SHELL_EXIT_FAILURE;
        }

    } else if (strcmp(argv[1], "~") == 0) {

        target = env_get("HOME");

        if (target == NULL) {
            print_message("cd: HOME not set");
            return SHELL_EXIT_FAILURE;
        }

    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        print_error("cd");
        return SHELL_EXIT_FAILURE;
    }

    /* Update PWD after changing directories. */
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        env_set("PWD", cwd);
    }

    return SHELL_EXIT_SUCCESS;
}

/* Print the current directory. */
static int builtin_pwd(char **argv, int argc)
{
    (void)argv;
    (void)argc;

    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        print_error("pwd");
        return SHELL_EXIT_FAILURE;
    }

    printf("%s\n", cwd);
    return SHELL_EXIT_SUCCESS;
}

/* Show command history. */
static int builtin_history(char **argv, int argc)
{
    (void)argv;
    (void)argc;

    history_print();
    return SHELL_EXIT_SUCCESS;
}

/* Display help information. */
static int builtin_help(char **argv, int argc)
{
    (void)argv;
    (void)argc;

    printf("%s - a Unix shell implemented in C\n\n", SHELL_NAME);
    printf("Built-in commands:\n");
    printf("  cd [dir]            change working directory (default: $HOME)\n");
    printf("  pwd                 print the current working directory\n");
    printf("  history             list the last %d commands\n", MAX_HISTORY);
    printf("  echo [args...]      print arguments, expanding $VARIABLES\n");
    printf("  export NAME=value   set and export an environment variable\n");
    printf("  unset NAME          remove an environment variable\n");
    printf("  clear               clear the terminal screen\n");
    printf("  exit [code]         exit the shell\n");
    printf("  help                show this message\n\n");

    printf("Features:\n");
    printf("  Pipelines:            cmd1 | cmd2 | cmd3\n");
    printf("  Input redirection:    cmd < file\n");
    printf("  Output redirection:   cmd > file   /   cmd >> file\n");
    printf("  Background execution: cmd &\n");
    printf("  History recall:       Up / Down arrow keys\n");

    return SHELL_EXIT_SUCCESS;
}

/* Clear the terminal screen. */
static int builtin_clear(char **argv, int argc)
{
    (void)argv;
    (void)argc;

    printf("\033[H\033[J");
    fflush(stdout);

    return SHELL_EXIT_SUCCESS;
}

/* Exit the shell. */
static int builtin_exit(char **argv, int argc)
{
    int code = SHELL_EXIT_SUCCESS;

    if (argc >= 2) {
        code = (int)strtol(argv[1], NULL, 10);
    }

    g_shell_state.running = 0;
    g_shell_state.last_exit_status = code;

    return code;
}

/* Print text to the terminal. */
static int builtin_echo(char **argv, int argc)
{
    int start = 1;
    int suppress_newline = 0;

    if (argc >= 2 && strcmp(argv[1], "-n") == 0) {
        suppress_newline = 1;
        start = 2;
    }

    /* Variables are already expanded by the parser. */
    for (int i = start; i < argc; i++) {
        if (i > start) {
            putchar(' ');
        }

        fputs(argv[i], stdout);
    }

    if (!suppress_newline) {
        putchar('\n');
    }

    fflush(stdout);
    return SHELL_EXIT_SUCCESS;
}

/* Set or list environment variables. */
static int builtin_export(char **argv, int argc)
{
    if (argc < 2) {
        /* Show all exported variables. */
        for (char **e = environ; *e != NULL; e++) {
            printf("export %s\n", *e);
        }

        return SHELL_EXIT_SUCCESS;
    }

    int status = SHELL_EXIT_SUCCESS;

    for (int i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');

        if (eq == NULL) {
            /* Export an existing variable. */
            if (env_get(argv[i]) == NULL) {
                env_set(argv[i], "");
            }

            continue;
        }

        size_t name_len = (size_t)(eq - argv[i]);

        char *name = xmalloc(name_len + 1);
        memcpy(name, argv[i], name_len);
        name[name_len] = '\0';

        const char *value = eq + 1;

        if (env_set(name, value) != 0) {
            print_error("export");
            status = SHELL_EXIT_FAILURE;
        }

        free(name);
    }

    return status;
}

/* Remove environment variables. */
static int builtin_unset(char **argv, int argc)
{
    if (argc < 2) {
        print_message("unset: usage: unset NAME [NAME ...]");
        return SHELL_EXIT_FAILURE;
    }

    int status = SHELL_EXIT_SUCCESS;

    for (int i = 1; i < argc; i++) {
        if (env_unset(argv[i]) != 0) {
            print_error("unset");
            status = SHELL_EXIT_FAILURE;
        }
    }

    return status;
}
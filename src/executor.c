/*
 * executor.c
 * Handles command execution, pipes, and I/O redirection.
 */

#include "executor.h"
#include "builtins.h"
#include "utils.h"
#include "shell.h"
#include "signals.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

/* Apply input/output redirection. */
static int apply_redirections(Command *cmd)
{
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd == -1) {
            print_error(cmd->input_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            print_error("dup2 (stdin)");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (cmd->append_mode ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output_file, flags, 0644);
        if (fd == -1) {
            print_error(cmd->output_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            print_error("dup2 (stdout)");
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

/* Close all pipe file descriptors. */
static void close_all_pipes(int pipefds[][2], int num_pipes)
{
    for (int i = 0; i < num_pipes; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }
}

/*
 * Run a command in a child process.
 * Sets up pipes, redirection, and executes the command.
 */
static void run_child_process(Command *cmd, int index, int num_commands,
                              int pipefds[][2], int num_pipes)
{
    if (index > 0) {
        dup2(pipefds[index - 1][0], STDIN_FILENO);
    }
    if (index < num_commands - 1) {
        dup2(pipefds[index][1], STDOUT_FILENO);
    }

    close_all_pipes(pipefds, num_pipes);

    /* Let Ctrl+C terminate the child normally. */
    signal(SIGINT, SIG_DFL);

    if (apply_redirections(cmd) != 0) {
        _exit(SHELL_EXIT_FAILURE);
    }

    if (is_builtin(cmd->argv[0])) {
        int status = execute_builtin(cmd->argv, cmd->argc);
        _exit(status);
    }

    execvp(cmd->argv[0], cmd->argv);

    /* execvp() returns only if it fails. */
    if (errno == ENOENT) {
        fprintf(stderr, "%s: %s: command not found\n",
                SHELL_NAME, cmd->argv[0]);
        _exit(127);
    }

    print_error(cmd->argv[0]);
    _exit(126);
}

/*
 * Run a single builtin in the shell process.
 * Needed for commands like cd and export.
 */
static int run_single_builtin_in_process(Command *cmd)
{
    int saved_stdin = -1;
    int saved_stdout = -1;

    if (cmd->input_file != NULL) {
        saved_stdin = dup(STDIN_FILENO);
    }
    if (cmd->output_file != NULL) {
        saved_stdout = dup(STDOUT_FILENO);
    }

    int status;
    if (apply_redirections(cmd) != 0) {
        status = SHELL_EXIT_FAILURE;
    } else {
        status = execute_builtin(cmd->argv, cmd->argc);
    }

    if (saved_stdin != -1) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }

    if (saved_stdout != -1) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    return status;
}

int execute_pipeline(Pipeline *pipeline)
{
    if (pipeline->num_commands <= 0) {
        return -1;
    }

    /* Run a standalone builtin without forking. */
    if (pipeline->num_commands == 1 &&
        !pipeline->background &&
        is_builtin(pipeline->commands[0].argv[0])) {

        int status = run_single_builtin_in_process(&pipeline->commands[0]);
        g_shell_state.last_exit_status = status;
        return status;
    }

    int num_commands = pipeline->num_commands;
    int num_pipes = num_commands - 1;

    /* Parser limits the maximum pipeline size. */
    int pipefds[MAX_PIPELINE_STAGES - 1][2];
    pid_t pids[MAX_PIPELINE_STAGES];

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds[i]) == -1) {
            print_error("pipe");

            /* Clean up any opened pipes. */
            close_all_pipes(pipefds, i);
            return -1;
        }
    }

    sigset_t old_mask;
    signals_block_sigchld(&old_mask);

    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            print_error("fork");
            close_all_pipes(pipefds, num_pipes);

            /* Wait for children already started. */
            for (int k = 0; k < i; k++) {
                int status;
                waitpid(pids[k], &status, 0);
            }

            signals_unblock_sigchld(&old_mask);
            return -1;
        }

        if (pid == 0) {
            /* Child process. */
            run_child_process(&pipeline->commands[i],
                              i,
                              num_commands,
                              pipefds,
                              num_pipes);
        }

        pids[i] = pid;
    }

    /* Parent no longer needs the pipe descriptors. */
    close_all_pipes(pipefds, num_pipes);

    if (pipeline->background) {
        for (int i = 0; i < num_commands; i++) {
            signals_register_background_pid(pids[i]);
        }

        printf("[background] pid %d\n", (int)pids[num_commands - 1]);
        fflush(stdout);

        /* SIGCHLD handler will clean up later. */
        signals_unblock_sigchld(&old_mask);
        return SHELL_EXIT_SUCCESS;
    }

    int last_status = SHELL_EXIT_SUCCESS;

    for (int i = 0; i < num_commands; i++) {
        int status;

        if (waitpid(pids[i], &status, 0) == -1) {
            if (errno == ECHILD) {
                /* Child already reaped. */
                continue;
            }
            continue;
        }

        if (i == num_commands - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            }
        }
    }

    signals_unblock_sigchld(&old_mask);

    g_shell_state.last_exit_status = last_status;
    return last_status;
}
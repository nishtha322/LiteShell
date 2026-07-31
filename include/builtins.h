/*
 * builtins.h
 * Declarations for built-in commands.
 */

#ifndef LITESHELL_BUILTINS_H
#define LITESHELL_BUILTINS_H

/* Check if a command is built-in. */
int is_builtin(const char *name);

/* Run a built-in command. */
int execute_builtin(char **argv, int argc);

#endif /* LITESHELL_BUILTINS_H */
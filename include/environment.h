/*
 * environment.h
 * Environment variable functions.
 */

#ifndef LITESHELL_ENVIRONMENT_H
#define LITESHELL_ENVIRONMENT_H

/* Initialize the environment module. */
void env_init(void);

/* Clean up the environment module. */
void env_cleanup(void);

/* Set an environment variable. */
int env_set(const char *name, const char *value);

/* Remove an environment variable. */
int env_unset(const char *name);

/* Get an environment variable. */
const char *env_get(const char *name);

/* Expand variables like $VAR and ${VAR}. */
char *expand_variables(const char *input);

#endif /* LITESHELL_ENVIRONMENT_H */
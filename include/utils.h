/*
 * utils.h
 * Common helper functions.
 */

#ifndef LITESHELL_UTILS_H
#define LITESHELL_UTILS_H

#include <stddef.h>

/* Allocate memory. */
void *xmalloc(size_t size);

/* Resize allocated memory. */
void *xrealloc(void *ptr, size_t size);

/* Duplicate a string. */
char *xstrdup(const char *s);

/* Remove leading and trailing spaces. */
char *trim_whitespace(char *s);

/* Check if a string is blank. */
int is_blank_string(const char *s);

/* Print a system error. */
void print_error(const char *context);

/* Print a shell message. */
void print_message(const char *message);

#endif /* LITESHELL_UTILS_H */
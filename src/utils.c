/*
 * utils.c
 * Common helper functions used across the shell.
 */

#include "utils.h"
#include "shell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

void *xmalloc(size_t size)
{
    /* Always allocate at least one byte. */
    void *ptr = malloc(size == 0 ? 1 : size);

    if (ptr == NULL) {
        fprintf(stderr, "%s: fatal: out of memory (requested %zu bytes)\n",
                SHELL_NAME, size);
        exit(SHELL_EXIT_FAILURE);
    }

    return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
    /* Always allocate at least one byte. */
    void *new_ptr = realloc(ptr, size == 0 ? 1 : size);

    if (new_ptr == NULL) {
        fprintf(stderr, "%s: fatal: out of memory (requested %zu bytes)\n",
                SHELL_NAME, size);
        exit(SHELL_EXIT_FAILURE);
    }

    return new_ptr;
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = xmalloc(len);
    memcpy(copy, s, len);
    return copy;
}

char *trim_whitespace(char *s)
{
    if (s == NULL) {
        return s;
    }

    /* Skip leading spaces. */
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }

    if (*s == '\0') {
        return s;
    }

    /* Remove trailing spaces. */
    char *end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return s;
}

int is_blank_string(const char *s)
{
    if (s == NULL) {
        return 1;
    }

    while (*s != '\0') {
        if (!isspace((unsigned char)*s)) {
            return 0;
        }
        s++;
    }

    return 1;
}

void print_error(const char *context)
{
    /* Print system error message. */
    char buffer[512];

    snprintf(buffer, sizeof(buffer), "%s: %s", SHELL_NAME, context);
    perror(buffer);
}

void print_message(const char *message)
{
    fprintf(stderr, "%s: %s\n", SHELL_NAME, message);
}
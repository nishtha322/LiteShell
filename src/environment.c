/*
 * environment.c
 * Handles environment variables and variable expansion.
 */

#include "environment.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void env_init(void)
{
    /* Nothing to initialize. */
}

void env_cleanup(void)
{
    /* Nothing to clean up. */
}

int env_set(const char *name, const char *value)
{
    return setenv(name, value, 1);
}

int env_unset(const char *name)
{
    return unsetenv(name);
}

const char *env_get(const char *name)
{
    return getenv(name);
}

/* Check if a character is valid in a variable name. */
static int is_identifier_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

char *expand_variables(const char *input)
{
    size_t input_len = strlen(input);

    /* Expand into a growing buffer. */
    size_t capacity = input_len + 64;
    size_t out_len = 0;
    char *result = xmalloc(capacity);

    size_t i = 0;

    while (i < input_len) {
        char c = input[i];

        if (c != '$') {
            /* Copy normal characters. */
            if (out_len + 1 >= capacity) {
                capacity *= 2;
                result = xrealloc(result, capacity);
            }

            result[out_len++] = c;
            i++;
            continue;
        }

        /* Parse the variable name. */
        size_t name_start;
        size_t name_end;
        int braced = 0;

        if (i + 1 < input_len && input[i + 1] == '{') {
            braced = 1;
            name_start = i + 2;
            name_end = name_start;

            while (name_end < input_len && input[name_end] != '}') {
                name_end++;
            }

            /* Treat an incomplete ${ as normal text. */
            if (name_end >= input_len) {
                braced = 0;
            }

        } else {
            name_start = i + 1;
            name_end = name_start;

            while (name_end < input_len &&
                   is_identifier_char(input[name_end])) {
                name_end++;
            }
        }

        /* '$' is not followed by a variable name. */
        if (name_end == name_start) {
            if (out_len + 1 >= capacity) {
                capacity *= 2;
                result = xrealloc(result, capacity);
            }

            result[out_len++] = '$';
            i++;
            continue;
        }

        /* Get the variable name. */
        size_t name_len = name_end - name_start;

        char *name = xmalloc(name_len + 1);
        memcpy(name, input + name_start, name_len);
        name[name_len] = '\0';

        const char *value = env_get(name);
        free(name);

        if (value != NULL) {
            size_t value_len = strlen(value);

            while (out_len + value_len + 1 >= capacity) {
                capacity *= 2;
                result = xrealloc(result, capacity);
            }

            memcpy(result + out_len, value, value_len);
            out_len += value_len;
        }

        /* Undefined variables become an empty string. */
        i = braced ? name_end + 1 : name_end;
    }

    result[out_len] = '\0';
    return result;
}
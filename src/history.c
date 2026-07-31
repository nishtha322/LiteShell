/*
 * history.c
 * Stores command history and saves it to ~/.liteshell_history.
 */

#include "history.h"
#include "shell.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

/* History stored as a circular buffer. */
static char *entries[MAX_HISTORY];
static int g_count = 0;   /* Number of stored commands */
static int g_start = 0;   /* Index of the oldest command */

/* Return the path of the history file. */
static char *history_file_path(void)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        if (pw != NULL) {
            home = pw->pw_dir;
        }
    }
    if (home == NULL || home[0] == '\0') {
        return NULL;
    }

    size_t needed = strlen(home) + strlen("/.liteshell_history") + 1;
    char *path = xmalloc(needed);
    snprintf(path, needed, "%s/.liteshell_history", home);
    return path;
}

void history_init(void)
{
    g_count = 0;
    g_start = 0;
    memset(entries, 0, sizeof(entries));

    char *path = history_file_path();
    if (path == NULL) {
        return;
    }

    FILE *fp = fopen(path, "r");
    free(path);

    if (fp == NULL) {
        /* History file doesn't exist yet. */
        return;
    }

    char line[MAX_HISTORY_LINE];

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);

        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        if (!is_blank_string(line)) {
            history_add(line);
        }
    }

    fclose(fp);
}

void history_cleanup(void)
{
    char *path = history_file_path();

    if (path != NULL) {
        FILE *fp = fopen(path, "w");

        if (fp != NULL) {
            for (int i = 0; i < g_count; i++) {
                int idx = (g_start + i) % MAX_HISTORY;
                fprintf(fp, "%s\n", entries[idx]);
            }

            fclose(fp);
        }

        free(path);
    }

    for (int i = 0; i < g_count; i++) {
        int idx = (g_start + i) % MAX_HISTORY;
        free(entries[idx]);
        entries[idx] = NULL;
    }

    g_count = 0;
    g_start = 0;
}

void history_add(const char *line)
{
    /* Ignore empty commands. */
    if (is_blank_string(line)) {
        return;
    }

    if (g_count > 0) {
        int last_idx = (g_start + g_count - 1) % MAX_HISTORY;

        /* Ignore consecutive duplicates. */
        if (strcmp(entries[last_idx], line) == 0) {
            return;
        }
    }

    if (g_count < MAX_HISTORY) {
        int idx = (g_start + g_count) % MAX_HISTORY;
        entries[idx] = xstrdup(line);
        g_count++;
    } else {
        /* Replace the oldest entry. */
        free(entries[g_start]);
        entries[g_start] = xstrdup(line);
        g_start = (g_start + 1) % MAX_HISTORY;
    }
}

int history_count(void)
{
    return g_count;
}

const char *history_get(int offset_from_newest)
{
    if (offset_from_newest < 0 || offset_from_newest >= g_count) {
        return NULL;
    }

    int idx = (g_start + g_count - 1 - offset_from_newest) % MAX_HISTORY;
    return entries[idx];
}

void history_print(void)
{
    for (int i = 0; i < g_count; i++) {
        int idx = (g_start + i) % MAX_HISTORY;
        printf("%5d  %s\n", i + 1, entries[idx]);
    }
}
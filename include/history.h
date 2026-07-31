/*
 * history.h
 * Command history functions.
 */

#ifndef LITESHELL_HISTORY_H
#define LITESHELL_HISTORY_H

/* Load command history. */
void history_init(void);

/* Save history and clean up. */
void history_cleanup(void);

/* Add a command to history. */
void history_add(const char *line);

/* Get the number of history entries. */
int history_count(void);

/* Get a command from history. */
const char *history_get(int offset_from_newest);

/* Print command history. */
void history_print(void);

#endif /* LITESHELL_HISTORY_H */
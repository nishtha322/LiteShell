/*
 * terminal.h
 * Terminal input functions.
 */

#ifndef LITESHELL_TERMINAL_H
#define LITESHELL_TERMINAL_H

/* Read a command line. */
char *terminal_read_line(void);

/* Restore terminal settings. */
void terminal_restore(void);

#endif /* LITESHELL_TERMINAL_H */
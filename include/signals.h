/*
 * signals.h
 * Signal handling functions.
 */

#ifndef LITESHELL_SIGNALS_H
#define LITESHELL_SIGNALS_H

#include <signal.h>

/* Set when Ctrl+C is pressed. */
extern volatile sig_atomic_t g_sigint_flag;

/* Initialize signal handlers. */
void signals_init(void);

#endif /* LITESHELL_SIGNALS_H */
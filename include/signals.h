/*
 * signals.h
 * Signal handling functions.
 */

#ifndef LITESHELL_SIGNALS_H
#define LITESHELL_SIGNALS_H

#include <signal.h>
#include <sys/types.h>

/* Set when Ctrl+C is pressed. */
extern volatile sig_atomic_t g_sigint_flag;

/* Initialize signal handlers. */
void signals_init(void);

/* Block SIGCHLD and save the previous signal mask into *old_mask. */
void signals_block_sigchld(sigset_t *old_mask);

/* Restore the signal mask saved by signals_block_sigchld(). */
void signals_unblock_sigchld(const sigset_t *old_mask);

/* Mark a pid as belonging to a background job. */
void signals_register_background_pid(pid_t pid);

#endif /* LITESHELL_SIGNALS_H */
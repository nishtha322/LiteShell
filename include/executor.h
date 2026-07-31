/*
 * executor.h
 * Executes parsed commands.
 */

#ifndef LITESHELL_EXECUTOR_H
#define LITESHELL_EXECUTOR_H

#include "shell.h"

/* Execute a pipeline. */
int execute_pipeline(Pipeline *pipeline);

#endif /* LITESHELL_EXECUTOR_H */
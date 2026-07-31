/*
 * parser.h
 * Parses user input into commands.
 */

#ifndef LITESHELL_PARSER_H
#define LITESHELL_PARSER_H

#include "shell.h"

/* Parse a command line. */
int parse_line(const char *line, Pipeline *pipeline);

/* Free a parsed pipeline. */
void free_pipeline(Pipeline *pipeline);

#endif /* LITESHELL_PARSER_H */
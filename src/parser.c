/*
 * parser.c
 * Tokenizes input and builds a command pipeline.
 */

#include "parser.h"
#include "environment.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKENS      1024
#define MAX_TOKEN_LEN   4096

/* Token produced by the lexer. */
typedef struct {
    char *text;
    int literal;   /* 1 if enclosed in single quotes */
} Token;

/* Lexer states. */
typedef enum {
    STATE_NORMAL,
    STATE_IN_SINGLE_QUOTE,
    STATE_IN_DOUBLE_QUOTE
} LexState;

/* Return true if the character is a shell operator. */
static int is_operator_char(char c)
{
    return c == '|' || c == '<' || c == '>' || c == '&';
}

/* Convert the input line into tokens. */
static int tokenize(const char *line, Token tokens[], int max_tokens)
{
    int num_tokens = 0;
    size_t i = 0;
    size_t len = strlen(line);

    char buffer[MAX_TOKEN_LEN];
    size_t buf_len = 0;

    /* Keeps track of the current token. */
    int have_token = 0;
    int token_literal = 1;
    LexState state = STATE_NORMAL;

    #define FLUSH_TOKEN()                                                   \
        do {                                                                \
            if (have_token) {                                               \
                if (num_tokens >= max_tokens) {                             \
                    print_message("too many tokens in input line");          \
                    for (int fi = 0; fi < num_tokens; fi++)                 \
                        free(tokens[fi].text);                              \
                    return -1;                                              \
                }                                                           \
                buffer[buf_len] = '\0';                                     \
                tokens[num_tokens].text = xstrdup(buffer);                  \
                tokens[num_tokens].literal = token_literal;                 \
                num_tokens++;                                               \
                buf_len = 0;                                                \
                have_token = 0;                                             \
                token_literal = 1;                                          \
            }                                                               \
        } while (0)

    #define APPEND_CHAR(ch)                                                 \
        do {                                                                \
            if (buf_len + 1 >= sizeof(buffer)) {                            \
                print_message("token exceeds maximum length");              \
                for (int fi = 0; fi < num_tokens; fi++)                     \
                    free(tokens[fi].text);                                  \
                return -1;                                                  \
            }                                                               \
            buffer[buf_len++] = (ch);                                       \
            have_token = 1;                                                 \
        } while (0)

    #define PUSH_OPERATOR(str)                                              \
        do {                                                                \
            FLUSH_TOKEN();                                                  \
            if (num_tokens >= max_tokens) {                                 \
                print_message("too many tokens in input line");             \
                for (int fi = 0; fi < num_tokens; fi++)                     \
                    free(tokens[fi].text);                                  \
                return -1;                                                  \
            }                                                               \
            tokens[num_tokens].text = xstrdup(str);                         \
            tokens[num_tokens].literal = 1;                                 \
            num_tokens++;                                                   \
        } while (0)

    while (i < len) {
        char c = line[i];

        switch (state) {

        case STATE_NORMAL:
            if (c == '\'') {
                state = STATE_IN_SINGLE_QUOTE;
                have_token = 1;
                i++;
            } else if (c == '"') {
                state = STATE_IN_DOUBLE_QUOTE;
                have_token = 1;
                token_literal = 0;
                i++;
            } else if (c == '\\') {
                /* Escape the next character. */
                if (i + 1 < len) {
                    APPEND_CHAR(line[i + 1]);
                    token_literal = 0;
                    i += 2;
                } else {
                    print_message("syntax error: trailing backslash");
                    for (int fi = 0; fi < num_tokens; fi++)
                        free(tokens[fi].text);
                    return -1;
                }
            } else if (c == ' ' || c == '\t') {
                FLUSH_TOKEN();
                i++;
            } else if (c == '>' && i + 1 < len && line[i + 1] == '>') {
                PUSH_OPERATOR(">>");
                i += 2;
            } else if (is_operator_char(c)) {
                char op[2] = { c, '\0' };
                PUSH_OPERATOR(op);
                i++;
            } else {
                APPEND_CHAR(c);
                token_literal = 0;
                i++;
            }
            break;

        case STATE_IN_SINGLE_QUOTE:
            if (c == '\'') {
                state = STATE_NORMAL;
                i++;
            } else {
                APPEND_CHAR(c);
                i++;
            }
            break;

        case STATE_IN_DOUBLE_QUOTE:
            if (c == '"') {
                state = STATE_NORMAL;
                i++;
            } else if (c == '\\' && i + 1 < len &&
                      (line[i + 1] == '"' ||
                       line[i + 1] == '\\' ||
                       line[i + 1] == '$')) {
                /* Escape special characters inside double quotes. */
                APPEND_CHAR(line[i + 1]);
                i += 2;
            } else {
                APPEND_CHAR(c);
                i++;
            }
            break;
        }
    }

    if (state != STATE_NORMAL) {
        print_message("syntax error: unterminated quote");
        for (int fi = 0; fi < num_tokens; fi++)
            free(tokens[fi].text);
        return -1;
    }

    FLUSH_TOKEN();

    #undef FLUSH_TOKEN
    #undef APPEND_CHAR
    #undef PUSH_OPERATOR

    return num_tokens;
}

/* Initialize a command structure. */
static void init_command(Command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
}

/* Build a pipeline from the token list. */
static int build_pipeline_from_tokens(Token tokens[],
                                      int num_tokens,
                                      Pipeline *pipeline)
{
    pipeline->num_commands = 0;
    pipeline->background = 0;

    Command current;
    init_command(&current);

    int stage_has_args = 0;

    for (int i = 0; i < num_tokens; i++) {
        const char *text = tokens[i].text;

        if (strcmp(text, "|") == 0) {

            if (!stage_has_args) {
                print_message("syntax error: empty command before '|'");
                init_command(&current);
                return -1;
            }

            current.argv[current.argc] = NULL;

            if (pipeline->num_commands >= MAX_PIPELINE_STAGES) {
                print_message("too many pipeline stages");
                return -1;
            }

            pipeline->commands[pipeline->num_commands++] = current;
            init_command(&current);
            stage_has_args = 0;

        } else if (strcmp(text, "<") == 0) {

            if (i + 1 >= num_tokens) {
                print_message("syntax error: expected filename after '<'");
                return -1;
            }

            i++;
            char *expanded = expand_variables(tokens[i].text);
            free(current.input_file);
            current.input_file = expanded;

        } else if (strcmp(text, ">") == 0 || strcmp(text, ">>") == 0) {

            int append = (strcmp(text, ">>") == 0);

            if (i + 1 >= num_tokens) {
                print_message("syntax error: expected filename after redirection");
                return -1;
            }

            i++;
            char *expanded = expand_variables(tokens[i].text);
            free(current.output_file);
            current.output_file = expanded;
            current.append_mode = append;

        } else if (strcmp(text, "&") == 0) {

            /* '&' is only allowed at the end. */
            if (i != num_tokens - 1) {
                print_message("syntax error: '&' must appear at the end of the line");
                return -1;
            }

            pipeline->background = 1;

        } else {

            /* Regular argument. */
            if (current.argc >= MAX_ARGS) {
                print_message("too many arguments in command");
                return -1;
            }

            char *value = tokens[i].literal
                            ? xstrdup(text)
                            : expand_variables(text);

            current.argv[current.argc++] = value;
            stage_has_args = 1;
        }
    }

    /* Nothing to execute. */
    if (!stage_has_args && pipeline->num_commands == 0) {
        return -1;
    }

    if (!stage_has_args) {
        print_message("syntax error: empty command after '|'");
        return -1;
    }

    current.argv[current.argc] = NULL;
    pipeline->commands[pipeline->num_commands++] = current;

    return 0;
}

int parse_line(const char *line, Pipeline *pipeline)
{
    memset(pipeline, 0, sizeof(*pipeline));

    Token tokens[MAX_TOKENS];

    int num_tokens = tokenize(line, tokens, MAX_TOKENS);
    if (num_tokens < 0) {
        return -1;
    }

    /* Ignore blank lines. */
    if (num_tokens == 0) {
        return -1;
    }

    int result = build_pipeline_from_tokens(tokens, num_tokens, pipeline);

    for (int i = 0; i < num_tokens; i++) {
        free(tokens[i].text);
    }

    if (result != 0) {
        free_pipeline(pipeline);
        memset(pipeline, 0, sizeof(*pipeline));
    }

    return result;
}

void free_pipeline(Pipeline *pipeline)
{
    if (pipeline == NULL) {
        return;
    }

    for (int i = 0; i < pipeline->num_commands; i++) {
        Command *cmd = &pipeline->commands[i];

        for (int a = 0; a < cmd->argc; a++) {
            free(cmd->argv[a]);
            cmd->argv[a] = NULL;
        }

        free(cmd->input_file);
        free(cmd->output_file);

        cmd->input_file = NULL;
        cmd->output_file = NULL;
        cmd->argc = 0;
    }

    pipeline->num_commands = 0;
}
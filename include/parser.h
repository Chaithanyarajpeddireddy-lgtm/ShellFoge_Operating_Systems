#ifndef PARSER_H
#define PARSER_H

#include "token.h"

#define MAX_COMMANDS 16
#define MAX_ARGS 64
#define MAX_PATH_LEN 256

typedef struct {
    int   argc;
    char *argv[MAX_ARGS];
    char  input[MAX_PATH_LEN];
    char  output[MAX_PATH_LEN];
    int   append;      // 0 = no, 1 = yes
    int   background;  // 0 = no, 1 = yes
} command_t;

typedef struct {
    command_t commands[MAX_COMMANDS];
    int command_count;
} pipeline_t;

void command_init(command_t *cmd);
int  parse(Token *tokens, pipeline_t *pipeline);
void pipeline_print(const pipeline_t *pipeline);
void pipeline_free(pipeline_t *pipeline);

#endif

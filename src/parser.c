#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/parser.h"

/* ---------- COMMAND_INIT ---------- */
void command_init(command_t *cmd) {
    cmd->argc = 0;
    cmd->input[0]  = '\0';
    cmd->output[0] = '\0';
    cmd->append = 0;
    cmd->background = 0;
    for (int i = 0; i < MAX_ARGS; i++) {
        cmd->argv[i] = NULL;
    }
}

/* ---------- PARSE ---------- */
int parse(Token *tokens, pipeline_t *pipeline) {
    pipeline->command_count = 1;
    int current = 0;

    command_init(&pipeline->commands[0]);

    Token *t = tokens;
    while (t != NULL && t->type != TOKEN_EOF) {

        switch (t->type) {

            case TOKEN_WORD:
                pipeline->commands[current].argv[pipeline->commands[current].argc++] = strdup(t->value);
                t = t->next;
                break;

            case TOKEN_REDIRECT_IN:
                if (t->next != NULL && t->next->type == TOKEN_WORD) {
                    strncpy(pipeline->commands[current].input, t->next->value, MAX_PATH_LEN - 1);
                    t = t->next->next;
                } else {
                    printf("Error: filename expected after <\n");
                    return 0;
                }
                break;

            case TOKEN_REDIRECT_OUT:
                if (t->next != NULL && t->next->type == TOKEN_WORD) {
                    strncpy(pipeline->commands[current].output, t->next->value, MAX_PATH_LEN - 1);
                    pipeline->commands[current].append = 0;
                    t = t->next->next;
                } else {
                    printf("Error: filename expected after >\n");
                    return 0;
                }
                break;

            case TOKEN_REDIRECT_APPEND:
                if (t->next != NULL && t->next->type == TOKEN_WORD) {
                    strncpy(pipeline->commands[current].output, t->next->value, MAX_PATH_LEN - 1);
                    pipeline->commands[current].append = 1;
                    t = t->next->next;
                } else {
                    printf("Error: filename expected after >>\n");
                    return 0;
                }
                break;

            case TOKEN_AMPERSAND:
                pipeline->commands[current].background = 1;
                t = t->next;
                break;

            case TOKEN_PIPE:
                pipeline->commands[current].argv[pipeline->commands[current].argc] = NULL;
                current++;
                if (current >= MAX_COMMANDS) {
                    printf("Error: Too many commands in pipeline.\n");
                    return 0;
                }
                command_init(&pipeline->commands[current]);
                pipeline->command_count++;
                t = t->next;
                break;

            default:
                t = t->next;
                break;
        }
    }

    pipeline->commands[current].argv[pipeline->commands[current].argc] = NULL;
    return 1;
}

/* ---------- PIPELINE_PRINT ---------- */
void pipeline_print(const pipeline_t *pipeline) {
    printf("\n========== PIPELINE ==========\n");

    for (int i = 0; i < pipeline->command_count; i++) {
        const command_t *cmd = &pipeline->commands[i];

        printf("\nCommand %d\n", i + 1);
        printf("--------------------------------\n");
        printf("Arguments\n");
        for (int j = 0; j < cmd->argc; j++) {
            printf("argv[%d] = %s\n", j, cmd->argv[j]);
        }
        printf("%-13s: %s\n", "Input",  cmd->input[0]  ? cmd->input  : "None");
        printf("%-13s: %s\n", "Output", cmd->output[0] ? cmd->output : "None");
        printf("%-13s: %s\n", "Append", cmd->append    ? "Yes" : "No");
        printf("%-13s: %s\n", "Background", cmd->background ? "Yes" : "No");
    }

    printf("================================\n");
}

/* ---------- PIPELINE_FREE ---------- */
void pipeline_free(pipeline_t *pipeline) {
    for (int i = 0; i < pipeline->command_count; i++) {
        command_t *cmd = &pipeline->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            free(cmd->argv[j]);
            cmd->argv[j] = NULL;
        }
    }
}

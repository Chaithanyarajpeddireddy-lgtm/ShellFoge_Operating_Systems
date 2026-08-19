#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/expand.h"

extern int last_exit_status;

static char *expand_word(const char *word) {
    char result[1024];
    result[0] = '\0';
    int ri = 0;

    for (int i = 0; word[i] != '\0'; i++) {
        if (word[i] == '~' && i == 0) {
            const char *home = getenv("HOME");
            if (home) {
                strncat(result, home, sizeof(result) - ri - 1);
                ri = strlen(result);
            } else {
                result[ri++] = word[i];
                result[ri] = '\0';
            }
            continue;
        }

        if (word[i] == '$') {
            if (word[i + 1] == '?') {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", last_exit_status);
                strncat(result, buf, sizeof(result) - ri - 1);
                ri = strlen(result);
                i++;
                continue;
            }
            if (isalpha((unsigned char)word[i + 1]) || word[i + 1] == '_') {
                char varname[128];
                int vi = 0;
                int j = i + 1;
                while (word[j] != '\0' &&
                       (isalnum((unsigned char)word[j]) || word[j] == '_') &&
                       vi < (int)sizeof(varname) - 1) {
                    varname[vi++] = word[j++];
                }
                varname[vi] = '\0';
                const char *val = getenv(varname);
                if (val) {
                    strncat(result, val, sizeof(result) - ri - 1);
                    ri = strlen(result);
                }
                i = j - 1;
                continue;
            }
        }

        result[ri++] = word[i];
        result[ri] = '\0';
    }

    return strdup(result);
}

void expand_variables(pipeline_t *pipeline) {
    for (int i = 0; i < pipeline->command_count; i++) {
        command_t *cmd = &pipeline->commands[i];
        for (int j = 0; j < cmd->argc; j++) {
            char *expanded = expand_word(cmd->argv[j]);
            free(cmd->argv[j]);
            cmd->argv[j] = expanded;
        }
        if (cmd->input[0]) {
            char *expanded = expand_word(cmd->input);
            strncpy(cmd->input, expanded, MAX_PATH_LEN - 1);
            free(expanded);
        }
        if (cmd->output[0]) {
            char *expanded = expand_word(cmd->output);
            strncpy(cmd->output, expanded, MAX_PATH_LEN - 1);
            free(expanded);
        }
    }
}

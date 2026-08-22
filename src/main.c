#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"

int last_exit_status = 0;

/* Runs a single non-builtin command: fork, wire up any redirection,
   execvp, and (unless backgrounded) wait for it to finish. */
static void execute_external(command_t *cmd) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        if (cmd->input[0]) {
            int fd_in = open(cmd->input, O_RDONLY);
            if (fd_in < 0) {
                perror(cmd->input);
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        if (cmd->output[0]) {
            int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
            int fd_out = open(cmd->output, flags, 0644);
            if (fd_out < 0) {
                perror(cmd->output);
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->argv[0]);
        exit(EXIT_FAILURE);
    }

    if (cmd->background) {
        printf("[background pid %d]\n", pid);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        }
    }
}

/* Runs every command in the pipeline. Returns 1 if the shell should stop. */
static int run_pipeline(pipeline_t *pipeline) {
    int should_exit = 0;

    for (int i = 0; i < pipeline->command_count; i++) {
        command_t *cmd = &pipeline->commands[i];
        if (cmd->argc == 0) continue;

        if (is_builtin(cmd->argv[0])) {
            builtin_status_t status = execute_builtin(cmd);
            if (status == BUILTIN_EXIT) {
                should_exit = 1;
            }
        } else {
            execute_external(cmd);
        }
    }

    return should_exit;
}

int main(void)
{
    printf("=====================================\n");
    printf("Shellforge \n");
    printf("BUILD and COMPILED by CHAITHANYA RAJ\n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");
    char *line;
    while (1)
    {
        line = readline("shellforge$ ");
        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }
        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }
        add_history(line);

        Token *tokens = lexer_tokenize(line);
        if (tokens != NULL && tokens->type == TOKEN_WORD && strcmp(tokens->value, "history") == 0) {
            HIST_ENTRY **the_list = history_list();
            if (the_list) {
                for (int i = 0; the_list[i]; i++) {
                    printf("  %d  %s\n", i + history_base, the_list[i]->line);
                }
            }
            token_free_list(tokens);
            free(line);
            continue;
        }
        Token *curr = tokens;
        printf("Tokens:\n");
        while (curr != NULL) {
            if (curr->value != NULL) {
                printf("  [%s]: %s\n", token_type_to_string(curr->type), curr->value);
            } else {
                printf("  [%s]\n", token_type_to_string(curr->type));
            }
            curr = curr->next;
        }

        pipeline_t pipeline;
        int should_exit = 0;
        if (parse(tokens, &pipeline)) {
            expand_variables(&pipeline);
            pipeline_print(&pipeline);
            should_exit = run_pipeline(&pipeline);
            pipeline_free(&pipeline);
        }

        token_free_list(tokens);
        free(line);

        if (should_exit) {
            break;
        }
    }
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "executor.h"
#include "jobs.h"

int last_exit_status = 0;

/* Job control setup (MILESTONE-5.1):
   - put the shell in its own process group and take the terminal for it,
   - ignore the signals a real interactive shell must ignore for itself:
       SIGTTOU/SIGTTIN so handing the terminal to a job and taking it back
       (tcsetpgrp) never stops the shell, and
       SIGTSTP/SIGINT/SIGQUIT so Ctrl+Z / Ctrl+C / Ctrl+\ at the prompt do
       not suspend or kill the shell - they only reach whichever job
       currently owns the terminal. Children reset these to default
       before exec (see executor.c). */
static void init_job_control(void) {
    shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

int main(void)
{
    setup_background_handler();
    init_job_control();
    jobs_init();

    printf("=====================================\n");
    printf("Shellforge \n");
    printf("BUILD and COMPILED by CHAITHANYA RAJ\n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");
    char *line;
    while (1)
    {
        /* Report background jobs that finished/stopped since the last prompt. */
        jobs_check_background();

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
        if (parse(tokens, &pipeline)) {
            expand_variables(&pipeline);
            pipeline_print(&pipeline);
            execute_pipeline(&pipeline);
            pipeline_free(&pipeline);
        }

        token_free_list(tokens);
        free(line);

        if (shell_exit_requested) {
            break;
        }
    }
    return 0;
}

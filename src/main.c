#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "token.h"
#include "lexer.h"

#include "parser.h"
#include "expand.h"

int last_exit_status = 0;

int main(void)
{
    // Display a welcome banner when the shell starts
    printf("=====================================\n");
    printf("Shellforge \n");
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
        if (strcmp(line, "exit") == 0)
        {
            free(line);
            printf("Exiting...\n");
            break;
        }

        // Tokenize and print the tokens
        Token *tokens = lexer_tokenize(line);

        // Check for built-in command "history"
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
        pipeline_free(&pipeline);
    }

        // Free the tokens
        token_free_list(tokens);

        free(line);
    }
    return 0;
}

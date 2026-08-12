#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>
#include "token.h"
#include "lexer.h"

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

        // Free the tokens
        token_free_list(tokens);

        free(line);
    }
    return 0;
}

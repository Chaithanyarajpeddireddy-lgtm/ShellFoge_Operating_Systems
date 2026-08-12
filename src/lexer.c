#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

Token *lexer_tokenize(const char *input) {
    Token *head = NULL;
    Token *tail = NULL;
    int i = 0;
    int len = strlen(input);

    while (i < len) {
        // Skip whitespace
        while (i < len && isspace((unsigned char)input[i])) {
            i++;
        }

        if (i >= len) {
            break;
        }

        char c = input[i];

        // Helper to append token to linked list
        #define APPEND_TOKEN(type, val) do { \
            Token *tok = token_create(type, val); \
            if (!head) { \
                head = tok; \
                tail = tok; \
            } else { \
                tail->next = tok; \
                tail = tok; \
            } \
        } while(0)

        // Check for special operators
        if (c == '|') {
            APPEND_TOKEN(TOKEN_PIPE, "|");
            i++;
        } else if (c == '&') {
            APPEND_TOKEN(TOKEN_AMPERSAND, "&");
            i++;
        } else if (c == '<') {
            APPEND_TOKEN(TOKEN_REDIRECT_IN, "<");
            i++;
        } else if (c == '>') {
            if (i + 1 < len && input[i + 1] == '>') {
                APPEND_TOKEN(TOKEN_REDIRECT_APPEND, ">>");
                i += 2;
            } else {
                APPEND_TOKEN(TOKEN_REDIRECT_OUT, ">");
                i++;
            }
        } else {
            // Read a word/argument (supporting single and double quotes)
            int start = i;
            char *word_buf = (char *)malloc(len - start + 1);
            int buf_idx = 0;
            int in_single_quote = 0;
            int in_double_quote = 0;

            while (i < len) {
                char curr_char = input[i];

                if (in_single_quote) {
                    if (curr_char == '\'') {
                        in_single_quote = 0;
                    } else {
                        word_buf[buf_idx++] = curr_char;
                    }
                    i++;
                } else if (in_double_quote) {
                    if (curr_char == '"') {
                        in_double_quote = 0;
                    } else {
                        word_buf[buf_idx++] = curr_char;
                    }
                    i++;
                } else {
                    if (curr_char == '\'') {
                        in_single_quote = 1;
                        i++;
                    } else if (curr_char == '"') {
                        in_double_quote = 1;
                        i++;
                    } else if (isspace((unsigned char)curr_char) || curr_char == '|' || curr_char == '&' || curr_char == '<' || curr_char == '>') {
                        break;
                    } else {
                        word_buf[buf_idx++] = curr_char;
                        i++;
                    }
                }
            }
            word_buf[buf_idx] = '\0';
            APPEND_TOKEN(TOKEN_WORD, word_buf);
            free(word_buf);
        }
        #undef APPEND_TOKEN
    }

    // Append EOF token
    Token *eof_tok = token_create(TOKEN_EOF, NULL);
    if (!head) {
        head = eof_tok;
    } else {
        tail->next = eof_tok;
    }

    return head;
}

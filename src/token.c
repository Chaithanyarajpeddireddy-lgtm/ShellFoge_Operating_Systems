#define _GNU_SOURCE
#include "token.h"
#include <stdlib.h>
#include <string.h>

Token *token_create(TokenType type, const char *value) {
    Token *tok = (Token *)malloc(sizeof(Token));
    if (!tok) return NULL;
    tok->type = type;
    if (value) {
        tok->value = strdup(value);
    } else {
        tok->value = NULL;
    }
    tok->next = NULL;
    return tok;
}

void token_free(Token *token) {
    if (!token) return;
    if (token->value) {
        free(token->value);
    }
    free(token);
}

void token_free_list(Token *head) {
    Token *curr = head;
    while (curr) {
        Token *next = curr->next;
        token_free(curr);
        curr = next;
    }
}

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_WORD: return "TOKEN_WORD";
        case TOKEN_PIPE: return "TOKEN_PIPE";
        case TOKEN_REDIRECT_IN: return "TOKEN_REDIRECT_IN";
        case TOKEN_REDIRECT_OUT: return "TOKEN_REDIRECT_OUT";
        case TOKEN_REDIRECT_APPEND: return "TOKEN_REDIRECT_APPEND";
        case TOKEN_AMPERSAND: return "TOKEN_AMPERSAND";
        case TOKEN_EOF: return "TOKEN_EOF";
        default: return "UNKNOWN";
    }
}

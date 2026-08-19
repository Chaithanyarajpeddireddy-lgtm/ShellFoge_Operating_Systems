#ifndef TOKEN_H
#define TOKEN_H

#define MAX_TOKENS 128
#define MAX_TOKEN_LEN 256

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,            // |
    TOKEN_REDIRECT_IN,     // <
    TOKEN_REDIRECT_OUT,    // >
    TOKEN_REDIRECT_APPEND, // >>
    TOKEN_AMPERSAND,       // &
    TOKEN_EOF
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
    struct Token *next;
} Token;

// Function prototypes
Token *token_create(TokenType type, const char *value);
void token_free(Token *token);
void token_free_list(Token *head);
const char *token_type_to_string(TokenType type);

#endif // TOKEN_H

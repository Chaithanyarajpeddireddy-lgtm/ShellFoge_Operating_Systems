#ifndef LEXER_H
#define LEXER_H

#include "token.h"

// Tokenizes the input string and returns a linked list of tokens
Token *lexer_tokenize(const char *input);

#endif // LEXER_H

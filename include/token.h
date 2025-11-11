#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    BEGIN = 1,
    END,
    INTEGER,
    IF,
    THEN,
    ELSE,
    FUNCTION,
    READ,
    WRITE,
    IDENT,
    CONST,
    EQU,
    NEQ,
    LE,
    LT,
    GE,
    GT,
    MINUS,
    MUL,
    ASSIGN,
    OPENPAREN,
    CLOSEPAREN,
    SEMICOLON,
    EOLN,
    _EOF,
} TokenType;

const char* get_token_name(TokenType);

#endif
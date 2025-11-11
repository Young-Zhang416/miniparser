#include "token.h"

const char* get_token_name(TokenType type) {
    switch (type) {
        case BEGIN: return "begin";
        case END: return "end";
        case INTEGER: return "integer";
        case IF: return "if";
        case THEN: return "then";
        case ELSE: return "else";
        case FUNCTION: return "function";
        case READ: return "read";
        case WRITE: return "write";
        case IDENT: return "ident";
        case CONST: return "const";
        case EQU: return "==";
        case NEQ: return "<>";
        case LE: return "<=";
        case LT: return "<";
        case GE: return ">=";
        case GT: return ">";
        case MINUS: return "-";
        case MUL: return "*";
        case ASSIGN: return ":=";
        case OPENPAREN: return "(";
        case CLOSEPAREN: return ")";
        case SEMICOLON: return ";";
        case EOLN: return "\\n";
        case _EOF: return "EOF";
        default: return "unknown";
    }
}
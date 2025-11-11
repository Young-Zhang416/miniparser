#ifndef PARSER_H
#define PARSER_H
#include "token.h"
#include "table.h"
#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define MAX_TOKENS 1024

typedef struct {
    TokenType type;
    char value[16];
} Token;

typedef struct Parser{
    Token* tokens;
    size_t token_count;
    size_t token_index;
    Token current_token;

    VariableEntry* vartab;
    size_t var_count;
    size_t var_capacity;

    ProcedureEntry* proctab;
    size_t proc_count;
    size_t proc_capacity;

    int current_level;
    char current_proc[16];
    int has_error;

    FILE* var;
    FILE* pro;
    FILE* err;

    int line_number;
} Parser;

void parser_error(Parser*, const char*);
bool is_valid_identifier(const char*);
Parser* create_parser(const char*);
void destroy_parser(Parser*);
void print_tokens(Parser*);
void next_token(Parser*);
TokenType current_token_type(Parser*);
TokenType peek_token_type(Parser*);
void consume_eoln(Parser*);
bool match(Parser*, TokenType);
bool program(Parser*);
void block(Parser*);
void declarations(Parser*);
void declaration(Parser*);
void var_declaration(Parser*);
void var(Parser*, int);
void func_declaration(Parser*);
void func(Parser*);
void executions(Parser*);
void execution(Parser*);
void parameter(Parser*);

void read_statement(Parser*);
void write_statement(Parser*);
void assignment_statement(Parser*);
void conditional_statement(Parser*);
void arithmetic_expression(Parser*);
void arithmetic_expression_prime(Parser*);
void term(Parser*);
void term_prime(Parser*);
void factor(Parser*);
void var_reference(Parser*);
void func_call(Parser*);
void parameter_list(Parser*);
void conditional_expression(Parser*);
void relation_operator(Parser*);
void output_to_file(Parser* p);

#endif
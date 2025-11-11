#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "parser.h"
#include "var.h"

Parser* create_parser(const char* filename) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (!parser) {
        perror("Failed to allocate parser");
        exit(EXIT_FAILURE);
    }

    char* filename_copy = strdup(filename);
    if (!filename_copy) {
        perror("Failed to copy filename");
        free(parser);
        exit(EXIT_FAILURE);
    }

    char* dot = strrchr(filename_copy, '.');
    if (!dot || strcmp(dot, ".dyd") != 0) {
        fprintf(stderr, "Error: input file must have a .dyd extension\n");
        free(filename_copy);
        free(parser);
        exit(EXIT_FAILURE);
    }

    FILE* file = fopen(filename_copy, "r");
    if (!file) {
        fprintf(stderr, "Error opening file %s\n", filename_copy);
        free(filename_copy);
        free(parser);
        exit(EXIT_FAILURE);
    }

    parser->tokens = malloc(MAX_TOKENS * sizeof(Token));
    if (!parser->tokens) {
        perror("Failed to allocate tokens");
        free(filename_copy);
        free(parser);
        exit(EXIT_FAILURE);
    }
    parser->token_count = 0;

    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, file)) != -1) {
        if (strcmp(line, "\n") == 0 || line[0] == '\n') {
            continue; // Skip empty lines
        }

        if (parser->token_count >= MAX_TOKENS) {
            fprintf(stderr, "tokens overflow\n");
            free(line);
            fclose(file);
            free(filename_copy);
            free(parser->tokens);
            free(parser);
            exit(EXIT_FAILURE);
        }

        Token* new_token = &parser->tokens[parser->token_count];
        const char* space = strchr(line, ' ');
        if (space == NULL) {
            fprintf(stderr, "Error: invalid line format\n");
            free(line);
            fclose(file);
            free(filename_copy);
            free(parser->tokens);
            free(parser);
            exit(EXIT_FAILURE);
        }

        size_t value_len = space - line;
        if (value_len >= sizeof(new_token->value)) {
            value_len = sizeof(new_token->value) - 1;
        }
        strncpy(new_token->value, line, value_len);
        new_token->value[value_len] = '\0';

        new_token->type = atoi(space + 1);

        parser->token_count++;
    }

    free(line);
    fclose(file);

    parser->token_index = 0;
    parser->current_token.type = _EOF;
    parser->current_token.value[0] = '\0';

    parser->var_capacity = 100;
    parser->var_count = 0;
    parser->vartab = (VariableEntry*)malloc(parser->var_capacity * sizeof(VariableEntry));
    if (!parser->vartab) {
        perror("Failed to allocate variable table");
        free(filename_copy);
        free(parser->tokens);
        free(parser);
        exit(EXIT_FAILURE);
    }

    parser->proc_capacity = 50;
    parser->proc_count = 0;
    parser->proctab = (ProcedureEntry*)malloc(parser->proc_capacity * sizeof(ProcedureEntry));
    if (!parser->proctab) {
        perror("Failed to allocate procedure table");
        free(filename_copy);
        free(parser->tokens);
        free(parser->vartab);
        free(parser);
        exit(EXIT_FAILURE);
    }

    parser->current_level = 0;
    strcpy(parser->current_proc, "main");
    parser->has_error = 0;
    parser->line_number = 1;

    *dot = '\0'; // Remove the extension

    char err_filename[256];
    char pro_filename[256];
    char var_filename[256];

    snprintf(err_filename, sizeof(err_filename), "%s.err", filename_copy);
    snprintf(pro_filename, sizeof(pro_filename), "%s.pro", filename_copy);
    snprintf(var_filename, sizeof(var_filename), "%s.var", filename_copy);

    free(filename_copy);

    parser->err = fopen(err_filename, "w");
    parser->pro = fopen(pro_filename, "w");
    parser->var = fopen(var_filename, "w");

    if (!parser->err || !parser->pro || !parser->var) {
        perror("Error opening output files");
        destroy_parser(parser);
        exit(EXIT_FAILURE);
    }

    return parser;
}

void destroy_parser(Parser* parser) {
    if (!parser) return;
    free(parser->tokens);
    free(parser->vartab);
    free(parser->proctab);
    if (parser->err) fclose(parser->err);
    if (parser->pro) fclose(parser->pro);
    if (parser->var) fclose(parser->var);
    free(parser);
}

// debug function to print tokens
void print_tokens(Parser* parser) {
    for (int i = 0; i < parser->token_count; i++) {
        printf("Token %d: value='%s', type=%d\n", i, parser->tokens[i].value, parser->tokens[i].type);
    }
}

void next_token(Parser* parser) {
    if (current_token_type(parser) == EOLN) {
        parser->line_number++;
    }
 
    if (parser->token_index < parser->token_count) {
        parser->current_token = parser->tokens[parser->token_index++];
   }
    else {
        parser->current_token.type = _EOF;
        parser->current_token.value[0] = '\0';
    }
}

TokenType current_token_type(Parser* parser) {
    return parser->current_token.type;
}

TokenType peek_token_type(Parser* parser) {
    if (parser->token_index < parser->token_count) {
        return parser->tokens[parser->token_index].type;
    }
    else {
        return _EOF;
    }
}

void consume_eoln(Parser* parser){
    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }
}

bool match(Parser* parser, TokenType type) {
    if (current_token_type(parser) == type) {
        next_token(parser);
        return true;
    }
    else {
        char error_msg[256];
        const char* expected_token_name = get_token_name(type);
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "Missing '%s' before '%s'", 
                 expected_token_name, actual_token_name);
        parser_error(parser, error_msg);
    }
    return false;
}




void parser_error(Parser* parser, const char* msg) {
    parser->has_error = 1;
    if (parser->err) {
        fprintf(parser->err, "LINE:%d %s\n", parser->line_number, msg);
    }
    fprintf(stderr, "LINE:%d %s\n", parser->line_number, msg);

    // clear the resources and exit
    destroy_parser(parser);

    exit(EXIT_FAILURE);
}

bool is_valid_identifier(const char* str) {
    if (!str || !isalpha((unsigned char)str[0])) return false;
    for (int i = 1; str[i] != '\0'; i++) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_') return false;
    }
    return true;
}

bool program(Parser* parser) {
    parser->has_error = 0;
    parser->line_number = 1;
    next_token(parser); // Initialize the first token
    block(parser);
    output_to_file(parser);
    return !parser->has_error;
}

void block(Parser* parser) {
    if (!match(parser, BEGIN) || !match(parser, EOLN)) {
        return;
    }

    declarations(parser);

    executions(parser);

    consume_eoln(parser);

    if (!match(parser, END)) {
        return;
    }

    consume_eoln(parser);
}

void declarations(Parser* parser) {
    while (current_token_type(parser) == INTEGER) {
        declaration(parser);
    }
}

void declaration(Parser* parser) {
    TokenType lookahead = peek_token_type(parser); // lookahead one token
    if (lookahead == FUNCTION) {
        func_declaration(parser);
    }
    else if (lookahead == IDENT) {
        var_declaration(parser); 
    }
    else {
        // only INTEGER followed by FUNCTION or IDENT is valid
        char error_msg[256];
        const char* lookahead_token_name = get_token_name(lookahead);
        snprintf(error_msg, sizeof(error_msg), 
                 "expected variable or function declaration but found '%s'", 
                 lookahead_token_name);
        parser_error(parser, error_msg);
    }
}

void var_declaration(Parser* parser) {
    var(parser, 0);
    if (!match(parser, SEMICOLON)) {
        return;
    }
    consume_eoln(parser);
}

void var(Parser* parser, int kind) {
    if (!match(parser, INTEGER)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected identifier but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    add_variable(parser, parser->current_token.value, VAR_INT, kind);
    match(parser, IDENT); // consume identifier
}

void func_declaration(Parser* parser) {
    if (!match(parser, INTEGER) || !match(parser, FUNCTION)) { // consume 'integer function'
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected function name but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    char func_name[16];
    strcpy(func_name, parser->current_token.value);

    int var_start = parser->var_count;
    match(parser, IDENT); // consume function name

    if (!match(parser, OPENPAREN)) {
        return;
    }

    char old_proc[16];
    strcpy(old_proc, parser->current_proc);
    strcpy(parser->current_proc, func_name);
    parser->current_level++;

    add_procedure(parser, func_name, var_start, -1); // -1 means not finalized yet

    parameter(parser);

    if (!match(parser, CLOSEPAREN)) {
        parser->current_level--;
        strcpy(parser->current_proc, old_proc);
        return;
    }

    if (!match(parser, SEMICOLON)) {
        parser->current_level--;
        strcpy(parser->current_proc, old_proc);
        return;
    }

    consume_eoln(parser);

    block(parser);

    int var_end = parser->var_count - 1;
    update_procedure(parser, func_name, var_start, var_end);

    strcpy(parser->current_proc, old_proc);
    parser->current_level--;
}

bool is_execution_token(TokenType type) {
    return type == READ || type == WRITE || type == IF || type == IDENT || type == BEGIN;
}

void executions(Parser* parser) {
    while (is_execution_token(current_token_type(parser))) {
        execution(parser);
    }
}

void execution(Parser* parser) {
    TokenType cur_type = current_token_type(parser);
    switch (cur_type) {
    case READ:
        read_statement(parser);
        break;

    case WRITE:
        write_statement(parser);
        break;

    case IF:
        conditional_statement(parser);
        break;

    case IDENT:
        assignment_statement(parser);
        break;

    case BEGIN:
        block(parser);
        break;

    default:
    {
        char error_msg[256];
        const char* actual_token_name = get_token_name(cur_type);
        snprintf(error_msg, sizeof(error_msg), 
                 "unexpected token '%s' in execution", 
                 actual_token_name);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void parameter(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        return; 
    }
    add_variable(parser, parser->current_token.value, VAR_UNKNOWN, 1);
    match(parser, IDENT);
}

void read_statement(Parser* parser) {
    if (!match(parser, READ)) {
        return;
    }

    if (!match(parser, OPENPAREN)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected identifier in read statement but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "variable '%s' not declared in procedure '%s'", 
                 parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        return;
    }

    match(parser, IDENT);

    if (!match(parser, CLOSEPAREN)) {
        return;
    }

    if (!match(parser, SEMICOLON)) {
        return;
    }

    consume_eoln(parser);
}

void write_statement(Parser* parser) {
    if (!match(parser, WRITE)) {
        return;
    }

    if (!match(parser, OPENPAREN)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected identifier in write statement but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "variable '%s' not declared in procedure '%s'", 
                 parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        return;
    }

    match(parser, IDENT);

    if (!match(parser, CLOSEPAREN)) {
        return;
    }

    if (!match(parser, SEMICOLON)) {
        return;
    }

    consume_eoln(parser);
}

void assignment_statement(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected identifier in assignment statement but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    bool is_return_assignment = (strcmp(parser->current_token.value, parser->current_proc) == 0);

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);

    if (var_entry == NULL && !is_return_assignment) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "variable '%s' not declared in procedure '%s'", 
                 parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        match(parser, IDENT);
        if (current_token_type(parser) == ASSIGN) {
            match(parser, ASSIGN);
            arithmetic_expression(parser);
        }
        match(parser, SEMICOLON);
        return;
    }

    match(parser, IDENT);
    if (!match(parser, ASSIGN)) {
        return;
    }

    arithmetic_expression(parser);
    if (!match(parser, SEMICOLON)) {
        return;
    }

    consume_eoln(parser);
}

void arithmetic_expression(Parser* parser) {
    term(parser);
    arithmetic_expression_prime(parser);
}

void arithmetic_expression_prime(Parser* parser) {
    if (current_token_type(parser) == MINUS) {
        match(parser, MINUS);
        term(parser);
        arithmetic_expression_prime(parser);
    }
}

void term(Parser* parser) {
    factor(parser);
    term_prime(parser);
}

void term_prime(Parser* parser) {
    if (current_token_type(parser) == MUL) {
        match(parser, MUL);
        factor(parser);
        term_prime(parser);
    }
}

void factor(Parser* parser) {
    TokenType cur_type = current_token_type(parser);
    switch (cur_type) {
    case IDENT:
        if (peek_token_type(parser) == OPENPAREN) {
            func_call(parser);
        }
        else {
            var_reference(parser);
        }
        break;

    case CONST:
        match(parser, CONST);
        break;

    case OPENPAREN:
        match(parser, OPENPAREN);
        arithmetic_expression(parser);
        match(parser, CLOSEPAREN);
        break;

    default:
    {
        char error_msg[256];
        const char* actual_token_name = get_token_name(cur_type);
        snprintf(error_msg, sizeof(error_msg), 
                 "unexpected token '%s' in factor", 
                 actual_token_name);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void var_reference(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected identifier in variable reference but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "variable '%s' not declared in procedure '%s'", 
                 parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        return;
    }

    match(parser, IDENT);
}

void func_call(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[256];
        const char* actual_token_name = get_token_name(current_token_type(parser));
        snprintf(error_msg, sizeof(error_msg), 
                 "expected function name in function call but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
        return;
    }

    ProcedureEntry* proc_entry = find_procedure(parser, parser->current_token.value);
    if (proc_entry == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "procedure '%s' not declared", 
                 parser->current_token.value);
        parser_error(parser, error_msg);
        return;
    }

    match(parser, IDENT);
    match(parser, OPENPAREN);
    parameter_list(parser);
    match(parser, CLOSEPAREN);
}

// only support single parameter for now
void parameter_list(Parser* parser) {
    arithmetic_expression(parser);
}

void conditional_statement(Parser* parser) {
    if (!match(parser, IF)) {
        return;
    }

    conditional_expression(parser);

    if (!match(parser, THEN)) {
        return;
    }

    consume_eoln(parser);

    executions(parser);

    if (current_token_type(parser) == ELSE) {
        match(parser, ELSE);
        consume_eoln(parser);
        executions(parser);
    }
}

void conditional_expression(Parser* parser) {
    arithmetic_expression(parser);
    relation_operator(parser);
    arithmetic_expression(parser);
}

void relation_operator(Parser* parser) {
    TokenType type = current_token_type(parser);

    switch (type) {
    case EQU:
    case NEQ:
    case LT:
    case GT:
    case GE:
    case LE:
        match(parser, type);
        break;
    default:
    {
        char error_msg[256];
        const char* actual_token_name = get_token_name(type);
        snprintf(error_msg, sizeof(error_msg), 
                 "expected relational operator but found '%s'", 
                 actual_token_name);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void output_to_file(Parser* p) {
    for (int i = 0; i < p->proc_count; i++) {
        ProcedureEntry* proc = &p->proctab[i];
        fprintf(p->pro, "%s %s %d %d %d\n",
            proc->pname, var_type_to_string(proc->ptype), proc->plev, proc->faddr, proc->laddr);
    }

    for (int i = 0; i < p->var_count; i++) {
        VariableEntry* var = &p->vartab[i];
        fprintf(p->var, "%s %s %d %s %d %d\n",
            var->vname, var->vproc, var->vkind, var_type_to_string(var->vtype),var->vlev, var->vaddr);
    }
}
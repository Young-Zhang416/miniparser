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
            fprintf(stderr, "Error: too many tokens\n");
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
    if (parser->token_index < parser->token_count) {
        parser->current_token = parser->tokens[parser->token_index++];

        if (current_token_type(parser) == EOLN) {
            parser->line_number++;
        }
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

bool match(Parser* parser, TokenType type) {
    if (current_token_type(parser) == type) {
        next_token(parser);
        return true;
    }
    else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected token type %d but found %d\n", parser->line_number, type, current_token_type(parser));
        parser_error(parser, error_msg);
    }
    return false;
}

void add_variable(Parser* parser, const char* value, VarType type, int kind) {
    if (!is_valid_identifier(value)) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: invalid identifier '%s'\n", parser->line_number, value);
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* exist = find_variable(parser, value, parser->current_proc);
    if (exist != NULL) {
        // 合并“函数体内声明”的形参类型定义：已存在且是参数（vkind=1），当前是普通变量声明（kind=0）
        if (exist->vkind == 1 && kind == 0) {
            exist->vtype = type; // 确认参数类型（VAR_UNKNOWN -> VAR_INT）
            return;
        }
        // 其它情况为重复声明错误
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: variable '%s' already declared in procedure '%s'\n", parser->line_number, value, parser->current_proc);
        parser_error(parser, error_msg);
        return;
    }

    if (parser->var_count >= parser->var_capacity) {
        parser->var_capacity *= 2;
        VariableEntry* new_vartab = (VariableEntry*)realloc(parser->vartab, parser->var_capacity * sizeof(VariableEntry));
        if (!new_vartab) {
            perror("Failed to reallocate variable table");
            parser_error(parser, "Error: failed to reallocate variable table\n");
            return;
        }
        parser->vartab = new_vartab;
    }

    VariableEntry* var_entry = &parser->vartab[parser->var_count];
    strncpy(var_entry->vname, value, sizeof(var_entry->vname) - 1);
    var_entry->vname[sizeof(var_entry->vname) - 1] = '\0';
    strncpy(var_entry->vproc, parser->current_proc, sizeof(var_entry->vproc) - 1);
    var_entry->vproc[sizeof(var_entry->vproc) - 1] = '\0';
    var_entry->vkind = kind; // 0 for variable, 1 for parameter
    var_entry->vtype = type;
    var_entry->vlev = parser->current_level;
    var_entry->vaddr = parser->var_count++; // Simple address allocation based on count
}

void add_procedure(Parser* parser, const char* name, int var_start, int var_end) {
    if (!is_valid_identifier(name)) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: invalid procedure name '%s'\n", parser->line_number, name);
        parser_error(parser, error_msg);
        return;
    }

    if (find_procedure(parser, name) != NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: procedure '%s' already declared\n", parser->line_number, name);
        parser_error(parser, error_msg);
        return;
    }

    if (parser->proc_count >= parser->proc_capacity) {
        parser->proc_capacity *= 2;
        ProcedureEntry* new_proctab = (ProcedureEntry*)realloc(parser->proctab, parser->proc_capacity * sizeof(ProcedureEntry));
        if (!new_proctab) {
            perror("Failed to reallocate procedure table");
            parser_error(parser, "Error: failed to reallocate procedure table\n");
            return;
        }
        parser->proctab = new_proctab;
    }

    ProcedureEntry* proc_entry = &parser->proctab[parser->proc_count];
    strncpy(proc_entry->pname, name, sizeof(proc_entry->pname) - 1);
    proc_entry->pname[sizeof(proc_entry->pname) - 1] = '\0';
    proc_entry->faddr = var_start;
    proc_entry->laddr = var_end; // -1 means not finalized yet
    proc_entry->plev = parser->current_level;
    proc_entry->ptype = VAR_FUNCTION;
    parser->proc_count++;
}

void update_procedure(Parser* parser, const char* name, int var_start, int var_end) {
    ProcedureEntry* proc = find_procedure(parser, name);
    if (!proc) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Internal Error: procedure '%s' not found for update\n", name);
        parser_error(parser, error_msg);
        return;
    }
    proc->faddr = var_start;
    proc->laddr = var_end;
}

void parser_error(Parser* parser, const char* msg) {
    parser->has_error = 1;
    if (parser->err) {
        fprintf(parser->err, "LINE:%d %s;\n", parser->line_number, msg);
    }
    fprintf(stderr, "LINE:%d %s;\n", parser->line_number, msg);
}

bool is_valid_identifier(const char* str) {
    if (!str || !isalpha((unsigned char)str[0])) return false;
    for (int i = 1; str[i] != '\0'; i++) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_') return false;
    }
    return true;
}

VariableEntry* find_variable(Parser* parser, const char* var_name, const char* proc_name) {
    for (int i = 0; i < parser->var_count; i++) {
        if (strcmp(parser->vartab[i].vname, var_name) == 0 &&
            strcmp(parser->vartab[i].vproc, proc_name) == 0) {
            return &parser->vartab[i];
        }
    }
    return NULL;
}

ProcedureEntry* find_procedure(Parser* parser, const char* proc_name) {
    for (int i = 0; i < parser->proc_count; i++) {
        if (strcmp(parser->proctab[i].pname, proc_name) == 0) {
            return &parser->proctab[i];
        }
    }
    return NULL;
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
    if (!match(parser, BEGIN)) {
        return;
    }

    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }

    declarations(parser);

    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }

    executions(parser);

    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }

    if (!match(parser, END)) {
        return;
    }
}

void declarations(Parser* parser) {
    while (current_token_type(parser) == INTEGER) {
        declaration(parser);

        while (current_token_type(parser) == EOLN) {
            next_token(parser);
        }
    }
}

void declaration(Parser* parser) {
    if (current_token_type(parser) != INTEGER) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected 'INTEGER' at the beginning of declaration but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    TokenType lookahead = peek_token_type(parser);
    if (lookahead == FUNCTION) {
        func_declaration(parser);
    }
    else if (lookahead == IDENT) {
        var_declaration(parser); 
    }
    else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected variable or function declaration but found token type %d\n", parser->line_number, lookahead);
        parser_error(parser, error_msg);
    }
}

void var_declaration(Parser* parser) {
    var(parser, 0);
    if (!match(parser, SEMICOLON)) {
        return;
    }
    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }
}

void var(Parser* parser, int kind) {
    if (!match(parser, INTEGER)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected identifier but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    add_variable(parser, parser->current_token.value, VAR_INT, kind);
    match(parser, IDENT);
}

void func_declaration(Parser* parser) {
    if (!match(parser, INTEGER) || !match(parser, FUNCTION)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected function name but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    char func_name[16];
    strcpy(func_name, parser->current_token.value);

    int var_start = parser->var_count;
    match(parser, IDENT); // function name
    if (!match(parser, OPENPAREN)) {
        return;
    }

    char old_proc[16];
    strcpy(old_proc, parser->current_proc);
    // 进入函数作用域
    strcpy(parser->current_proc, func_name);
    parser->current_level++;

    // 提前登记，支持递归
    add_procedure(parser, func_name, var_start, -1);

    // 形参（单个）
    parameter(parser);

    if (!match(parser, CLOSEPAREN)) {
        parser->current_level--;
        strcpy(parser->current_proc, old_proc);
        return;
    }

    // 函数头分号
    if (!match(parser, SEMICOLON)) {
        parser->current_level--;
        strcpy(parser->current_proc, old_proc);
        return;
    }

    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }

    // 函数体
    block(parser);

    int var_end = parser->var_count - 1;
    update_procedure(parser, func_name, var_start, var_end);

    // 离开函数作用域
    strcpy(parser->current_proc, old_proc);
    parser->current_level--;
}

bool is_execution_token(TokenType type) {
    return type == READ || type == WRITE || type == IF || type == IDENT || type == BEGIN;
}

void executions(Parser* parser) {
    while (is_execution_token(current_token_type(parser))) {
        execution(parser);
        while (current_token_type(parser) == EOLN) {
            next_token(parser);
        }
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
        // 块语句作为语句出现，不需要分号
        block(parser);
        break;

    default:
    {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: unexpected token type %d in execution\n", parser->line_number, cur_type);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void parameter(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        return; // 允许无参，但当前语言只支持单参；此处无参直接返回
    }
    // 形参：vkind=1，类型未知，待函数体内 integer 声明确定
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
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected identifier in read statement but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: variable '%s' not declared in procedure '%s'\n", parser->line_number, parser->current_token.value, parser->current_proc);
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
}

void write_statement(Parser* parser) {
    if (!match(parser, WRITE)) {
        return;
    }

    if (!match(parser, OPENPAREN)) {
        return;
    }

    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected identifier in write statement but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: variable '%s' not declared in procedure '%s'\n", parser->line_number, parser->current_token.value, parser->current_proc);
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
}

void assignment_statement(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected identifier in assignment statement but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    // 函数名可作为返回变量
    bool is_return_assignment = (strcmp(parser->current_token.value, parser->current_proc) == 0);

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);

    if (var_entry == NULL && !is_return_assignment) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: variable '%s' not declared in procedure '%s'\n", parser->line_number, parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        // 同步：尽量消费本条语句
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
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: unexpected token type %d in factor\n", parser->line_number, cur_type);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void var_reference(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected identifier in variable reference but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* var_entry = find_variable(parser, parser->current_token.value, parser->current_proc);
    if (var_entry == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: variable '%s' not declared in procedure '%s'\n", parser->line_number, parser->current_token.value, parser->current_proc);
        parser_error(parser, error_msg);
        return;
    }

    match(parser, IDENT);
}

void func_call(Parser* parser) {
    if (current_token_type(parser) != IDENT) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected function name in function call but found token type %d\n", parser->line_number, current_token_type(parser));
        parser_error(parser, error_msg);
        return;
    }

    ProcedureEntry* proc_entry = find_procedure(parser, parser->current_token.value);
    if (proc_entry == NULL) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Error at line %d: procedure '%s' not declared\n", parser->line_number, parser->current_token.value);
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

    // THEN 后解析单个语句，前后跳过 EOLN
    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }
    execution(parser);

    while (current_token_type(parser) == EOLN) {
        next_token(parser);
    }

    if (current_token_type(parser) == ELSE) {
        match(parser, ELSE);
        while (current_token_type(parser) == EOLN) {
            next_token(parser);
        }
        execution(parser);
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
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Syntax Error at line %d: expected relational operator but found token type %d\n", parser->line_number, type);
        parser_error(parser, error_msg);
    }
    break;
    }
}

void output_to_file(Parser* p) {
    for (int i = 0; i < p->proc_count; i++) {
        ProcedureEntry* proc = &p->proctab[i];
        fprintf(p->pro, "%s %d %d %d %d\n",
            proc->pname, proc->ptype, proc->plev, proc->faddr, proc->laddr);
    }

    for (int i = 0; i < p->var_count; i++) {
        VariableEntry* var = &p->vartab[i];
        fprintf(p->var, "%s %s %d %d %d %d\n",
            var->vname, var->vproc, var->vtype, var->vkind, var->vlev, var->vaddr);
    }
}
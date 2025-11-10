#ifndef TABLE_H
#define TABLE_H
#include "token.h"

typedef struct {
    char vname[16]; // variable name
    char vproc[16]; // procedure name
    int vkind; // 0 for variable, 1 for procedure
    TokenType vtype; // type of the variable (e.g., int, float, etc.)
    int vlev; // level of the variable
    int vaddr; // address of the variable
} VariableEntry;

typedef struct {
    char pname[16];
    TokenType ptype; // type of the procedure (e.g., int, void, etc.)
    int plev; // level of the procedure
    int faddr; // address of the first variable in the procedure
    int laddr; // address of the last variable in the procedure
} ProcedureEntry;

#endif
#ifndef TABLE_H
#define TABLE_H

#include "token.h"
#include "var.h"

// Forward declaration of Parser to avoid circular dependency
struct Parser;

typedef struct {
    char vname[16]; // variable name
    char vproc[16]; // procedure name
    int vkind; // 0 for variable, 1 for procedure
    VarType vtype; // type of the variable (e.g., int, float, etc.)
    int vlev; // level of the variable
    int vaddr; // address of the variable
} VariableEntry;

typedef struct {
    char pname[16];
    VarType ptype; // type of the return value of procedure (e.g., int, void, etc.)
    int plev; // level of the procedure
    int faddr; // address of the first variable in the procedure
    int laddr; // address of the last variable in the procedure
} ProcedureEntry;

void add_variable(struct Parser*, const char*, VarType, int);
void add_procedure(struct Parser*, const char*, int, int);
void update_procedure(struct Parser*, const char*, int, int);

VariableEntry* find_variable(struct Parser*, const char*, const char*);
ProcedureEntry* find_procedure(struct Parser*, const char*);

#endif
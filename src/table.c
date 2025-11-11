#include "parser.h"
#include <stdio.h>

void add_variable(Parser* parser, const char* value, VarType type, int kind) {
    if (!is_valid_identifier(value)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "invalid identifier '%s'", 
                 value);
        parser_error(parser, error_msg);
        return;
    }

    VariableEntry* exist = find_variable(parser, value, parser->current_proc);
    if (exist != NULL) {
        if (exist->vkind == 1 && kind == 0) {
            exist->vtype = type; 
            return;
        }
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "variable '%s' already declared in procedure '%s'", 
                 value, parser->current_proc);
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
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "invalid procedure name '%s'", 
                 name);
        parser_error(parser, error_msg);
        return;
    }

    if (find_procedure(parser, name) != NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "procedure '%s' already declared", 
                 name);
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
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                 "Internal Error: procedure '%s' not found for update", 
                 name);
        parser_error(parser, error_msg);
        return;
    }
    proc->faddr = var_start;
    proc->laddr = var_end;
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
#ifndef VAR_H
#define VAR_H

typedef enum {
    VAR_INT = 0,
    VAR_FUNCTION,
    VAR_UNKNOWN,
}VarType;

const char* var_type_to_string(VarType type);

#endif
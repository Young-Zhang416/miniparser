#include "var.h"

const char* var_type_to_string(VarType type){
    switch(type){
        case VAR_INT:
            return "integer";
        case VAR_FUNCTION:
            return "function";
        default:
            return "unknown";
    }
    return "invalid";
}
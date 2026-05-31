#pragma once

#include "parser.h"

struct Type_Checker {
    Parser *p_parser;

    void check();
    void check_user_defined_type_completeness();
    void resolve_global_variable_types();
};

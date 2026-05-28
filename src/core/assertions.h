#pragma once

#include <print>
#include <debug_break.h>
#include "escape_codes.h"

#ifdef NDEBUG
// In release mode it ignores the argument and does nothing.
#   define debug_assert(x) ((void)0)
#else
// In debug mode, if the expression is false, it stops on the debugger (if available) and then crashes.
#   define debug_assert(x)\
    do {\
        if (!(x)) {\
            eprintln("\n" ESC_CODE_RED "Assertion failed" ESC_CODE_RESET);\
            eprint_path(__FILE__, __LINE__, 1);\
            eprintln(": The expression \"{}\" is false.", #x);\
            debug_break();\
            std::exit(1);\
        }\
    } while(0)
#endif

#define unreachable()\
do {\
    eprint("\n");\
    eprint_path(__FILE__, __LINE__, 1);\
    eprintln(": " ESC_CODE_RED "Unreachable reached" ESC_CODE_RESET);\
    debug_break();\
    std::exit(1);\
} while(0)


#define panic(format_string, ...)\
    do {\
        eprintln("\n" ESC_CODE_RED "Panic" ESC_CODE_RESET);\
        eprint_path(__FILE__, __LINE__, 1);\
        eprintln(": " format_string, ##__VA_ARGS__);\
        debug_break();\
        std::exit(1);\
    } while(0)

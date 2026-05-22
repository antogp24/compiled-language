#pragma once

#include <print>

// See https://gist.github.com/JBlond/2fea43a3049b38287e5e9cefc87b2124
// ------------------------------------------------------------------- //

#define ESC_CODE_RESET "\x1b[0m"

// Regular Colors
#define ESC_CODE_BLACK  "\x1b[30m"
#define ESC_CODE_RED    "\x1b[31m"
#define ESC_CODE_GREEN  "\x1b[32m"
#define ESC_CODE_YELLOW "\x1b[33m"
#define ESC_CODE_BLUE   "\x1b[34m"
#define ESC_CODE_PURPLE "\x1b[35m"
#define ESC_CODE_CYAN   "\x1b[36m"
#define ESC_CODE_WHITE  "\x1b[37m"

// Bold Colors
#define ESC_CODE_BLACK_BOLD  "\x1b[1;30m"
#define ESC_CODE_RED_BOLD    "\x1b[1;31m"
#define ESC_CODE_GREEN_BOLD  "\x1b[1;32m"
#define ESC_CODE_YELLOW_BOLD "\x1b[1;33m"
#define ESC_CODE_BLUE_BOLD   "\x1b[1;34m"
#define ESC_CODE_PURPLE_BOLD "\x1b[1;35m"
#define ESC_CODE_CYAN_BOLD   "\x1b[1;36m"
#define ESC_CODE_WHITE_BOLD  "\x1b[1;37m"

// Underline Colors
#define ESC_CODE_BLACK_UNDERLINE  "\x1b[4;30m"
#define ESC_CODE_RED_UNDERLINE    "\x1b[4;31m"
#define ESC_CODE_GREEN_UNDERLINE  "\x1b[4;32m"
#define ESC_CODE_YELLOW_UNDERLINE "\x1b[4;33m"
#define ESC_CODE_BLUE_UNDERLINE   "\x1b[4;34m"
#define ESC_CODE_PURPLE_UNDERLINE "\x1b[4;35m"
#define ESC_CODE_CYAN_UNDERLINE   "\x1b[4;36m"
#define ESC_CODE_WHITE_UNDERLINE  "\x1b[4;37m"

// Bold Background Colors
#define ESC_CODE_BLACK_BOLD_BG  "\x1b[1;40m"
#define ESC_CODE_RED_BOLD_BG    "\x1b[1;41m"
#define ESC_CODE_GREEN_BOLD_BG  "\x1b[1;42m"
#define ESC_CODE_YELLOW_BOLD_BG "\x1b[1;43m"
#define ESC_CODE_BLUE_BOLD_BG   "\x1b[1;44m"
#define ESC_CODE_PURPLE_BOLD_BG "\x1b[1;45m"
#define ESC_CODE_CYAN_BOLD_BG   "\x1b[1;46m"
#define ESC_CODE_WHITE_BOLD_BG  "\x1b[1;47m"

// Helpers that use the escape codes.
// ------------------------------------------------------------------- //

#define eprint(format_string, ...) std::print(stderr, format_string, ##__VA_ARGS__)
#define eprintln(format_string, ...) std::println(stderr, format_string, ##__VA_ARGS__)

#define eprint_path(filename, line, column)\
    eprint(ESC_CODE_BLUE_UNDERLINE "{}:{}:{}" ESC_CODE_RESET,\
        (filename), (line), (column))

#define eprintln_path(filename, line, column)\
    eprintln(ESC_CODE_BLUE_UNDERLINE "{}:{}:{}" ESC_CODE_RESET,\
        (filename), (line), (column))

#define print_path(filename, line, column)\
    std::print(ESC_CODE_BLUE_UNDERLINE "{}:{}:{}" ESC_CODE_RESET,\
        (filename), (line), (column))


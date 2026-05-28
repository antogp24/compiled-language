#include "lexer.h"
#include "parser.h"
#include "type_checker.h"

std::string_view get_exe_name(std::string_view executable_path);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        eprintln("usage: {} code.txt", get_exe_name(argv[0]));
        std::exit(1);
    }
    const char* code_path = argv[1];
    Lexer lexer(String_View::from_cstr(code_path));
    lexer.lex();
    Parser parser(&lexer);
    parser.parse();
    parser.print_results();
    Type_Checker type_checker = { .p_parser = &parser };
    type_checker.check();
}

std::string_view get_exe_name(std::string_view executable_path)
{
    const size_t len = executable_path.size();
    Option<size_t> last_slash_index = {};
    for (size_t i = 0; i < len; ++i) {
        char c = executable_path[i];
        if (c == '/' || c == '\\') {
            last_slash_index = Some(i);
        }
    }
    if (last_slash_index.is_some()) {
        size_t start = last_slash_index.unwrap() + 1;
        return executable_path.substr(start, len - start);
    }
    return executable_path;
}
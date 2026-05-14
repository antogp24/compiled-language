#include "lexer.h"

std::string_view get_exe_name(std::string_view executable_path);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        eprintln("usage: {} code.txt", get_exe_name(argv[0]));
        std::exit(1);
    }
    std::string_view code_path = argv[1];
    Lexer lexer(code_path);
    lexer.lex();
    lexer.print_token_stream();
}

std::string_view get_exe_name(std::string_view executable_path)
{
    const size_t len = executable_path.size();
    Option<size_t> last_slash_index = None(size_t);
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
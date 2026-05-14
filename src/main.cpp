#include "lexer.h"

int main(void)
{
    Lexer lexer("examples/helloworld.txt");
    lexer.lex();
    lexer.print_token_stream();
}
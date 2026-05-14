#include "lexer.h"
#include <fstream>
#include <unordered_map>

std::string read_entire_file(std::string_view path)
{
    std::ifstream file((std::string)path, std::ios::binary);
    if (!file) {
        std::println(
            ESC_CODE_RED_BOLD "error" ESC_CODE_RESET
            ESC_CODE_RED ": Failed to open file \"{}\"." ESC_CODE_RESET, path);
        std::exit(1);
    }

    file.seekg(0, std::ios::end);
    std::size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');
    file.read(buffer.data(), size);

    return buffer;
}

size_t get_digit_count(size_t x)
{
    if (x == 0) {
        return 1;
    }
    return (size_t)std::floor(std::log10(x)) + 1;
}

void pretty_print_line(std::string_view line, Location location)
{
    size_t line_number_digit_count = get_digit_count(location.line);
    eprintln("{:>{}} | ", ' ', line_number_digit_count);
    eprintln("{} | {}\n", location.line, line);
    eprintln("{:>{}} | {:>{}}", ' ', line_number_digit_count, '^', location.column + 1);
};


// I don't use isspace because it has undefined behaviour for certain inputs.
bool is_whitespace(char c)
{
    switch (c) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
        return true;
    }
    return false;
}

bool is_alphabetic(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool is_numeric(char c)
{
    return c >= '0' && c <= '9';
}

Token_Kind get_keyword(std::string_view text)
{
    // O(n) implementation.

    // With a hash map I can achieve O(1), but the on the standard library allocates memory on the heap.
    // The perfect solution would be to create a Hash Map that stores it's data on static memory,
    // but the amount of keywords is so small that this is good enough.

    struct Pair {
        const char *key;
        Token_Kind value;
    };
    static Pair table[] = {
        {.key = "break", .value = Token_Kind::Break },
        {.key = "cast", .value = Token_Kind::Cast },
        {.key = "continue", .value = Token_Kind::Continue },
        {.key = "else", .value = Token_Kind::Else },
        {.key = "enum", .value = Token_Kind::Enum },
        {.key = "fn", .value = Token_Kind::Fn },
        {.key = "for", .value = Token_Kind::For },
        {.key = "if", .value = Token_Kind::If },
        {.key = "let", .value = Token_Kind::Let },
        {.key = "return", .value = Token_Kind::Return },
        {.key = "struct", .value = Token_Kind::Struct },
        {.key = "union", .value = Token_Kind::Union },
        {.key = "while", .value = Token_Kind::While },
        {.key = "void", .value = Token_Kind::Void },
        {.key = "bool", .value = Token_Kind::Bool },
        {.key = "char", .value = Token_Kind::Char },
        {.key = "string", .value = Token_Kind::String },
        {.key = "f32", .value = Token_Kind::F32 },
        {.key = "f64", .value = Token_Kind::F64 },
        {.key = "i8", .value = Token_Kind::I8 },
        {.key = "i16", .value = Token_Kind::I16 },
        {.key = "i32", .value = Token_Kind::I32 },
        {.key = "i64", .value = Token_Kind::I64 },
        {.key = "isize", .value = Token_Kind::Isize },
        {.key = "u8", .value = Token_Kind::U8 },
        {.key = "u16", .value = Token_Kind::U16 },
        {.key = "u32", .value = Token_Kind::U32 },
        {.key = "u64", .value = Token_Kind::U64 },
        {.key = "usize", .value = Token_Kind::Usize },
        {.key = "vec2", .value = Token_Kind::Vec2 },
        {.key = "vec3", .value = Token_Kind::Vec3 },
        {.key = "vec4", .value = Token_Kind::Vec4 },
        {.key = "mat4", .value = Token_Kind::Mat4 },
    };
    for (const Pair &pair : table) {
        if (text.compare(pair.key) == 0) {
            return pair.value;
        }
    }
    return Token_Kind::None;
}

bool is_builtin(Token_Kind kind)
{
    using enum Token_Kind;
    switch (kind) {
    case Void:
    case Bool:
    case Char:
    case String:
    case F32:
    case F64:
    case I8:
    case I16:
    case I32:
    case I64:
    case Isize:
    case U8:
    case U16:
    case U32:
    case U64:
    case Usize:
    case Vec2:
    case Vec3:
    case Vec4:
    case Mat4:
        return true;
    }
    return false;
}

Token_ID Lexer::push_token(Token_Kind kind, Location loc)
{
    Token_ID id = token_pool.allocate();
    Token &token = token_pool.get(id);
    token.kind = kind;
    token.location = loc;
    return id;
}

Token_ID Lexer::push_token(Token_Kind kind, Location loc, Token_Data data)
{
    Token_ID id = token_pool.allocate();
    Token &token = token_pool.get(id);
    token.kind = kind;
    token.location = loc;
    token.data = data;
    return id;
}

void Lexer::advance(size_t count)
{
    for (size_t i = 0; !is_eof() && i < count; ++i) {
        if (source[cursor] == '\n') {
            std::string_view this_line = source.substr(current_line_start, cursor - current_line_start);
            line_map.insert({ location.line, this_line });
            location.line++;
            location.column = 0;
            current_line_start = cursor + 1;
        } else {
            location.column++;
        }
        cursor++;
    }
}

void Lexer::skip_whitespace()
{
    while (!is_eof()) {
        char c = source[cursor];
        if (is_whitespace(c)) {
            advance(1);
        } else {
            return;
        }
    }
}

void Lexer::print_error_message_line(Location error_location)
{
    eprintln_path(filename, error_location);

    if (line_map.contains(error_location.line)) {
        pretty_print_line(line_map[error_location.line], error_location);
    } else {
        while (!is_eof() && source[cursor] != '\n') {
            advance(1);
        }
        advance(1); // Consume the \n, this also updates the line map.

        // Now it should contain the line.
        if (line_map.contains(error_location.line)) {
            pretty_print_line(line_map[error_location.line], error_location);
        }
    }
}

void Lexer::print_token_stream()
{
    for (const Token &token : token_pool) {
        print_path(filename, token.location);
        std::println(": {}", token);
    }
}

void Lexer::lex()
{
#define push_single(token)\
do {\
    push_token(Token_Kind::token, location);\
    advance(1);\
} while(0)

#define push_if(extra_char, token_if_extra, token_else)\
do{\
    if (peek_next() == Some(extra_char)) {\
        push_token(Token_Kind::token_if_extra, location);\
        advance(2);\
    } else {\
        push_token(Token_Kind::token_else, location);\
        advance(1);\
    }\
}while (0)

#define push_op(repeated_char, op_op_equal, op_op, op_equal, op)\
do{\
    Option<char> next = peek_next();\
    if (next.is_some()) {\
        switch (next.unwrap()) {\
        case repeated_char: {\
            advance(1);\
            if (peek_next() == Some('=')) {\
                push_token(Token_Kind::op_op_equal, location);\
                advance(2);\
            } else {\
                push_token(Token_Kind::op_op, location);\
                advance(1);\
            }\
        } break;\
        case '=': {\
            push_token(Token_Kind::op_equal, location);\
            advance(2);\
        } break;\
        }\
    } else {\
        push_token(Token_Kind::op, location);\
        advance(1);\
    }\
}while (0)

    for (;;) {
        skip_whitespace();
        if (is_eof()) {
            break;
        }
        char c = source[cursor];
        if (c == '_' || is_alphabetic(c)) {
            lex_identifier();
        }
        else if (is_numeric(c)) {
            lex_number_literal();
        }
        else {
            switch (c) {
            case '\'': lex_char_literal(); break;
            case '\"': lex_string_literal(); break;
            case '#': push_single(Hash); break;
            case '$': push_single(DollarSign); break;
            case '(': push_single(ParenLeft); break;
            case ')': push_single(ParenRight); break;
            case '[': push_single(BracketLeft); break;
            case ']': push_single(BracketRight); break;
            case '{': push_single(BraceLeft); break;
            case '}': push_single(BraceRight); break;
            case '~': push_if('=', BitwiseNotEqual, BitwiseNot); break;
            case '|': push_op('|', OrEqual, Or, BitwiseOrEqual, BitwiseOr); break;
            case '&': push_op('&', AndEqual, And, BitwiseAndEqual, Ampersand); break;
            case '^': push_if('=', BitwiseXorEqual, BitwiseXor); break;
            case '+': push_if('=', PlusEqual, Plus); break;
            case '-': push_if('=', MinusEqual, Minus); break;
            case '*': push_if('=', StarEqual, Star); break;
            case '%': push_if('=', ModuloEqual, Modulo); break;
            case '<': push_op('<', ShiftLeftEqual, ShiftLeft, LessEqual, LessThan); break;
            case '>': push_op('>', ShiftRightEqual, ShiftRight, GreaterEqual, GreaterThan); break;
            case '!': push_if('=', NotEqual, Not); break;
            case '=': push_if('=', EqualEqual, Equal); break;
            case '.': push_single(Dot); break;
            case ',': push_single(Comma); break;
            case ':': push_single(Colon); break;
            case ';': push_single(Semicolon); break;

            default:
                error_at(*this, location, "Unexpected character {}", c);
            }
        }
    }
    push_token(Token_Kind::EndOfFile, location);

#undef push_single
#undef push_if
#undef push_op
}

void Lexer::lex_identifier()
{
    feature_todo(*this, location, "Identifiers");
}

void Lexer::lex_number_literal()
{
    feature_todo(*this, location, "Number literal");
}

void Lexer::lex_char_literal()
{
    feature_todo(*this, location, "Char literal");
}

void Lexer::lex_string_literal()
{
    feature_todo(*this, location, "String literal");
}

void Lexer::lex_slash()
{
    feature_todo(*this, location, "Slash");
}

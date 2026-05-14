#include "lexer.h"
#include <fstream>
#include <unordered_map>
#include <charconv>

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
    eprintln("{} | {}", location.line, line);
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

bool is_hexadecimal_letter(char c)
{
    return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

bool is_decimal_digit(char c)
{
    return (c >= '0') && (c <= '9');
}

bool is_binary_digit(char c)
{
    return (c == '0') || (c == '1');
}

bool is_octal_digit(char c)
{
    return (c >= '0') && (c <= '7');
}

bool is_hexadecimal_digit(char c)
{
    return is_hexadecimal_letter(c) || is_decimal_digit(c);
}

bool is_base_compatible_with(Number_Base base, Number_Base other)
{
    return (int)base >= (int)other;
}

Number_Base get_digit_base(char c)
{
    using enum Number_Base;

    if (is_binary_digit(c)) {
        return Binary;
    } else if (is_octal_digit(c)) {
        return Octal;
    } else if (is_decimal_digit(c)) {
        return Decimal;
    } else if (is_hexadecimal_digit(c)) {
        return Hexadecimal;
    }
    return None;
}

bool is_alphanumeric(char c)
{
    return is_alphabetic(c) || is_decimal_digit(c);
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

double Lexer::parse_f64(std::string_view number_text, Location loc)
{
    double value = 0.0;
    std::from_chars_result result = std::from_chars(
        number_text.data(),
        number_text.data() + number_text.size(),
        value);

#pragma warning(push)
#pragma warning(disable : 4063)
    switch (result.ec) {
    case std::errc():
        // No error, the expected branch.
        break;
    case std::errc::invalid_argument:
        error_at(*this, loc, "Failed to parse float, this is not a number: \"{}\"", number_text);
        break;
    case std::errc::result_out_of_range:
        error_at(*this, loc, "Failed to parse float, it is larger than what a 64-bit float supports: {}", number_text);
        break;
    default:
        error_at(*this, loc, "Failed to parse float: \"{}\"", number_text);
        break;
    }
    return value;
#pragma warning(pop)
}

uint64_t Lexer::parse_u64(std::string_view number_text, Location loc, int base)
{
    uint64_t value = 0;
    std::from_chars_result result = std::from_chars(
        number_text.data(),
        number_text.data() + number_text.size(),
        value,
        base);

#pragma warning(push)
#pragma warning(disable : 4063)
    switch (result.ec) {
    case std::errc():
        // No error, the expected branch.
        break;
    case std::errc::invalid_argument:
        error_at(*this, loc, "Failed to parse int, this is not a number: \"{}\"", number_text);
        break;
    case std::errc::result_out_of_range:
        error_at(*this, loc, "Failed to parse int, it is larger than what a 64-bit unsigned integer supports: {}", number_text);
        break;
    default:
        error_at(*this, loc, "Failed to parse int: \"{}\"", number_text);
        break;
    }
    return value;
#pragma warning(pop)
}

std::string Lexer::unescape(std::string_view text, Location loc)
{
    // As a reference, take a look at this:
    // https://gist.github.com/ConnerWill/d4b6c776b509add763e17f9f113fd25b

    const size_t len = text.size();
    std::string unescaped = {};

    for (size_t i = 0; i < len;) {
        if (text[i] == '\\') {
            i++; // consume the '\\'
            if (i >= len) {
                error_at(*this, loc, "Unterminated escape sequence.");
            }
            switch (text[i]) {
            case '\'': unescaped.push_back('\''); i++; break; // Single Quote
            case '\"': unescaped.push_back('\"'); i++; break; // Double Quote
            case 'a': unescaped.push_back('\a'); i++; break; // Terminal Bell
            case 'b': unescaped.push_back('\b'); i++; break; // Backspace
            case 't': unescaped.push_back('\t'); i++; break; // Horizontal TAB
            case 'v': unescaped.push_back('\v'); i++; break; // Vertical TAB
            case 'n': unescaped.push_back('\n'); i++; break; // Linefeed
            case 'f': unescaped.push_back('\f'); i++; break; // Formfeed
            case 'r': unescaped.push_back('\r'); i++; break; // Carriage return
            case 'x': {
                i++; // consume the 'x'
                size_t start = i;
                while (i < len && is_hexadecimal_digit(text[i])) {
                    i++;
                }
                std::string_view hex_number_text = text.substr(start, i - start);
                uint64_t hex = parse_u64(hex_number_text, loc, 16);
                char least_significant_byte = (char)(hex & 0xff);
                unescaped.push_back(least_significant_byte);
            } break;

            default:
                error_at(*this, loc, "Unsupported/Undefined escape sequence.");
            }
        } else {
            unescaped.push_back(text[i]);
            i++;
        }
    }
    return unescaped;
}

void Lexer::advance(size_t count)
{
    for (size_t i = 0; !is_eof() && i < count; ++i) {
        if (source[cursor] == '\n') {
            std::string_view this_line = slice_source(current_line_start, cursor);
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
        advance(1); // consume the \n, this also updates the line map.

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
        } else if (is_decimal_digit(c)) {
            lex_number_literal();
        } else {
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
    Location loc = location;
    size_t start = cursor;

    advance(1); // consume the first character of the identifier.
    while (!is_eof()) {
        char c = source[cursor];
        if (c == '_' || is_alphanumeric(c)) {
            advance(1);
        } else {
            break;
        }
    }
    std::string_view identifier_name = slice_source(start, cursor);
    Token_Kind kind = get_keyword(identifier_name);

    if (kind == Token_Kind::None) {
        push_token(Token_Kind::Identifier, loc, Token_Data{ .str = identifier_name });
    } else {
        push_token(kind, loc); // It's a keyword.
    }
}

void Lexer::consume_digits(Number_Base base)
{
    // TODO: Support scientific exponentiation.
    while (!is_eof() && is_alphanumeric(source[cursor])) {
        char c = source[cursor];
        Number_Base c_base = get_digit_base(c);
        if (!is_base_compatible_with(base, c_base)) {
            error_at(*this, location, "The character '{}' is not in {} base.", c, magic_enum::enum_name(base));
        }
        advance(1);
    }
}

void Lexer::lex_number_literal()
{
    Location loc = location;
    size_t start = cursor;

    Number_Base base = Number_Base::Decimal; // The default is decimal.

    // Handling support 0x, 0b and 0o prefixes for integers.
    if (source[cursor] == '0') {
        advance(1); // consume the leading 0.
        char prefix = source[cursor];
        switch (prefix) {
        case 'b': base = Number_Base::Binary; break;
        case 'o': base = Number_Base::Octal; break;
        case 'x': base = Number_Base::Hexadecimal; break;
        default:
            error_at(*this, loc, "Unrecognized integer prefix \"0{}\"", prefix);
        }
        advance(1); // consume the base prefix.
    }

    consume_digits(base); // leading digits.

    if (!is_eof() && source[cursor] == '.') {
        advance(1); // consume the dot.
        consume_digits(base); // trailing digits.

        std::string_view float_number_text = slice_source(start, cursor);
        double float_value = parse_f64(float_number_text, loc);
        push_token(Token_Kind::Float_Literal, loc, Token_Data{ .float_literal = float_value });
    } else {
        std::string_view integer_number_text = slice_source(start, cursor);
        uint64_t integer_value = parse_u64(integer_number_text, loc, (int)base);
        push_token(Token_Kind::Int_Literal, loc, Token_Data{ .int_literal = integer_value });
    }
}

void Lexer::lex_char_literal()
{
    Location loc = location;
    Assert(source[cursor] == '\'');
    advance(1); // consume the first '

    size_t start = cursor;
    while (!is_eof() && source[cursor] != '\'') {
        if (source[cursor] == '\\') {
            advance(1); // consume first character of escape sequence (could be the ')
        }
        advance(1);
    }
    if (is_eof()) {
        error_at(*this, loc, "Unterminated Char literal.");
    }

    std::string_view escaped_char_text = slice_source(start, cursor);
    std::string unescaped = unescape(escaped_char_text, loc);

    if (unescaped.size() == 0) {
        error_at(*this, loc, "Empty Char literal is invalid.");
    } else if (unescaped.size() > 1) {
        error_at(*this, loc, "More than one character in Char literal.");
    }
    char char_literal = unescaped[0];
    push_token(Token_Kind::Char_Literal, loc, Token_Data{ .char_literal = char_literal });

    Assert(source[cursor] == '\'');
    advance(1); // consume the last '\''
}

void Lexer::lex_string_literal()
{
    feature_todo(*this, location, "String literal");
}

void Lexer::lex_slash()
{
    feature_todo(*this, location, "Slash");
}

#pragma once

#include <print>
#include <cstdint>
#include <cstdlib> // std::exit
#include <string>
#include <magic_enum/magic_enum.hpp>
#include <debug_break.h>
#include "core/pool.h"
#include "core/option.h"
#include "core/string.h"
#include "core/escape_codes.h"

struct Location {
    size_t byte_offset;
    uint32_t line;
    uint32_t column;
};

enum class Token_Kind {
    None,
    EndOfFile,
    Identifier,

    // Keywords
    // NOTE: If you modify this, please modify get_keyword() too.
    Break,
    Cast,
    Const,
    Continue,
    Else,
    Enum,
    False,
    Fn,
    For,
    If,
    Let,
    Loop,
    Null,
    Return,
    Struct,
    True,
    Union,
    While,

    // Built-in Types (are also keywords)
    // NOTE: If you modify this, please modify get_keyword() and is_builtin() too.
    Void,
    Bool,
    Char,
    String,
    F32,
    F64,
    I8,
    I16,
    I32,
    I64,
    Isize,
    U8,
    U16,
    U32,
    U64,
    Usize,
    Vec2,
    Vec3,
    Vec4,
    Mat4,

    // Literals
    Int_Literal,
    Float_Literal,
    Char_Literal,
    String_Literal,

    // Symbols
    Arrow,           // ->
    Hash,            // #
    HashQuote,       // #'
    DollarSign,      // $
    ParenLeft,       // (
    ParenRight,      // )
    BracketLeft,     // [
    BracketRight,    // ]
    BraceLeft,       // {
    BraceRight,      // }
    BitwiseNot,      // ~
    BitwiseOr,       // |
    Ampersand,       // &
    BitwiseXor,      // ^
    BitwiseNotEqual, // ~=
    BitwiseOrEqual,  // |=
    BitwiseAndEqual, // &=
    BitwiseXorEqual, // ^=
    Plus,            // +
    Minus,           // -
    Star,            // *
    Div,             // /
    Modulo,          // %
    PlusEqual,       // +=
    MinusEqual,      // -=
    StarEqual,       // *=
    DivEqual,        // /=
    ModuloEqual,     // %=
    LessThan,        // <
    GreaterThan,     // >
    ShiftLeft,       // <<
    ShiftRight,      // >>
    ShiftLeftEqual,  // <<=
    ShiftRightEqual, // >>=
    Not,             // !
    And,             // &&
    Or,              // ||
    Equal,           // =
    EqualEqual,      // ==
    NotEqual,        // !=
    AndEqual,        // &&=
    OrEqual,         // ||=
    LessEqual,       // <=
    GreaterEqual,    // >=
    Dot,             // .
    Comma,           // ,
    Colon,           // :
    Semicolon,       // ;
};

enum class Number_Base : int {
    None = 0,
    Binary = 2,
    Octal = 8,
    Decimal = 10,
    Hexadecimal = 16,
};

std::string escape_char(char c);
std::string escape_string(String_View text);
std::string read_entire_file(String_View path);
size_t get_digit_count(size_t x);
void pretty_print_line(String_View line, Location location);
bool is_whitespace(char c);
bool is_alphabetic(char c);
bool is_hexadecimal_letter(char c);
bool is_decimal_digit(char c);
bool is_binary_digit(char c);
bool is_octal_digit(char c);
bool is_hexadecimal_digit(char c);
bool is_base_compatible_with(Number_Base base, Number_Base other);
Number_Base get_digit_base(char c);
bool is_alphanumeric(char c);
Token_Kind get_keyword(String_View text);
bool is_builtin(Token_Kind kind);

// NOTE: Since the string literal depends on the lifetime of the input file source code,
// the heap allocated data for that buffer must live for the entire lifetime of the compiler.
union Token_Variant {
    String_View str; // A view into the source code file's string.
    uint64_t int_literal;
    double float_literal;
    char char_literal;
};

struct Token {
    Token_Variant data;
    Location location;
    Token_Kind kind;
};

template <>
struct std::formatter<Token> {
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.begin();
    }

    auto format(const Token &token, std::format_context &ctx) const {
        switch (token.kind) {
        case Token_Kind::Identifier:
            return std::format_to(ctx.out(),
                "Token{{kind=Identifier, name=\"{}\"}}",
                token.data.str);
        case Token_Kind::String_Literal:
            return std::format_to(ctx.out(),
                "Token{{kind=String_Literal, text=\"{}\"}}",
                token.data.str);
        case Token_Kind::Char_Literal:
            return std::format_to(ctx.out(),
                "Token{{kind=Char_Literal, value=\'{}\'}}",
                escape_char(token.data.char_literal));
        case Token_Kind::Int_Literal:
            return std::format_to(ctx.out(),
                "Token{{kind=Int_Literal, value={}}}",
                token.data.int_literal);
        case Token_Kind::Float_Literal:
            return std::format_to(ctx.out(),
                "Token{{kind=Float_Literal, value={}}}",
                token.data.float_literal);
        }
        return std::format_to(ctx.out(),
            "Token{{kind={}}}",
            magic_enum::enum_name(token.kind));
    }
};

#define feature_todo(lexer, location, feature_name)\
do {\
    eprintln("\n" ESC_CODE_RED_BOLD_BG "TODO" ESC_CODE_RESET ":"\
        ESC_CODE_RED " Feature \"{}\" is unimplemented." ESC_CODE_RESET,\
        (feature_name));\
    (lexer).print_error_message_line(location);\
    eprint(ESC_CODE_YELLOW "INFO" ESC_CODE_RESET ": implementation should go here: ");\
    eprintln_path(__FILE__, __LINE__, 1);\
    eprintln("");\
    debug_break();\
    std::exit(1);\
} while(0)

#define error_at(lexer, location, format_string, ...)\
do {\
    eprint("\n" ESC_CODE_RED_BOLD_BG "error" ESC_CODE_RESET ":" ESC_CODE_RED " ");\
    eprint(format_string, ##__VA_ARGS__);\
    eprintln(ESC_CODE_RESET);\
    (lexer).print_error_message_line(location);\
    eprintln("");\
    debug_break();\
    std::exit(1);\
} while(0)

#define error_unlocated(format_string, ...)\
do {\
    eprint("\n" ESC_CODE_RED_BOLD_BG "error" ESC_CODE_RESET ":" ESC_CODE_RED " ");\
    eprint(format_string, ##__VA_ARGS__);\
    eprintln(ESC_CODE_RESET);\
    debug_break();\
    std::exit(1);\
} while(0)

using Token_Pool = Pool<Token, 1024>;

struct Lexer {
    std::string buffer_of_source_code = {}; // Fuck std::string::substr, it returns a std::string instead of String_View.
    Token_Pool token_pool = {}; // An ordered pool of tokens.
    String_View source = {}; // A view into the buffer for the source code.
    String_View filename = {}; // Path of the source code's file.
    Location current_location = { .byte_offset = 0, .line = 1, .column = 1 };

    Lexer(String_View path)
        : filename(path)
    {
        buffer_of_source_code = read_entire_file(path);
        source.data = buffer_of_source_code.data();
        source.length = buffer_of_source_code.size();
    }

    constexpr char peek() const
    {
        return source[current_location.byte_offset];
    }

    constexpr char peek_at(size_t byte_offset) const
    {
        return source[byte_offset];
    }

    constexpr Option<char> peek_next() const
    {
        if (current_location.byte_offset + 1 < source.length) {
            return Some(source[current_location.byte_offset + 1]);
        }
        return {};
    }

    constexpr bool is_eof() const
    {
        return current_location.byte_offset >= source.length;
    }

    constexpr bool is_eof_at(size_t byte_offset) const
    {
        return byte_offset >= source.length;
    }

    void push_token(Token_Kind kind, Location loc);
    void push_token(Token_Kind kind, Location loc, Token_Variant data);
    double parse_f64(String_View number_text, Location loc);
    uint64_t parse_u64(String_View number_text, Location loc, int base);
    std::string unescape(String_View text, Location loc);
    void advance(size_t count);
    void skip_whitespace();
    String_View get_line(Location loc);
    void print_error_message_line(Location error_location);
    void print_token_stream();
    void lex();
    void lex_identifier();
    void consume_digits(Number_Base base);
    void lex_number_literal();
    void lex_char_literal();
    void lex_string_literal();
    void lex_multiline_comment();
    void lex_slash();
};

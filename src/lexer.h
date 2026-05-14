#pragma once

#include <print>
#include <cstdint>
#include <cstdlib> // std::exit
#include <string>
#include <string_view>
#include <magic_enum/magic_enum.hpp>
#include "core/pool.h"
#include "core/option.h"

struct Location {
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
    Continue,
    Else,
    Enum,
    Fn,
    For,
    If,
    Let,
    Return,
    Struct,
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
    Hash,            // #
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

std::string read_entire_file(std::string_view path);
size_t get_digit_count(size_t x);
void pretty_print_line(std::string_view line, Location location);
bool is_whitespace(char c);
bool is_alphabetic(char c);
bool is_numeric(char c);
Token_Kind get_keyword(std::string_view text);
bool is_builtin(Token_Kind kind);

// NOTE: Since the string literal depends on the lifetime of the input file source code,
// the heap allocated data for that buffer must live for the entire lifetime of the compiler.
union Token_Data {
    std::string_view str; // A view into the source code file's string.
    int64_t int_literal;
    double float_literal;
    char char_literal;
};

struct Token {
    Token_Data data;
    Location location;
    Token_Kind kind;
};

using Token_ID = Pool_ID;

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
                token.data.char_literal);
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

#define ESC_CODE_RESET      "\x1b[0m"
#define ESC_CODE_UNDERSCORE "\x1b[4m"
#define ESC_CODE_RED        "\x1b[31m"
#define ESC_CODE_YELLOW     "\x1b[33m"
#define ESC_CODE_BLUE       "\x1b[34m"
#define ESC_CODE_RED_BOLD   "\x1b[41;1m"

#define eprint(format_string, ...) std::print(stderr, format_string, ##__VA_ARGS__)
#define eprintln(format_string, ...) std::println(stderr, format_string, ##__VA_ARGS__)

#define eprintln_path(filename, location)\
    eprintln(ESC_CODE_BLUE ESC_CODE_UNDERSCORE "{}:{}:{}" ESC_CODE_RESET,\
        (filename), (location).line, (location).column + 1)

#define print_path(filename, location)\
    std::print(ESC_CODE_BLUE ESC_CODE_UNDERSCORE "{}:{}:{}" ESC_CODE_RESET,\
        (filename), (location).line, (location).column + 1)

#define feature_todo(lexer, location, feature_name)\
do {\
    eprintln("\n" ESC_CODE_RED_BOLD "TODO" ESC_CODE_RESET ":"\
        ESC_CODE_RED " Feature {} is unimplemented" ESC_CODE_RESET,\
        (feature_name));\
    (lexer).print_error_message_line(location);\
    eprint(ESC_CODE_YELLOW "INFO" ESC_CODE_RESET ": implementation should go here: ");\
    Location location_in_compiler = { .line = (uint32_t)__LINE__, .column = 0 };\
    eprintln_path(__FILE__, location_in_compiler);\
    eprintln("");\
    std::exit(1);\
} while(0)

#define error_at(lexer, location, format_string, ...)\
do {\
    eprint("\n" ESC_CODE_RED_BOLD "error" ESC_CODE_RESET ":" ESC_CODE_RED " ");\
    eprint(format_string, ##__VA_ARGS__);\
    eprintln(ESC_CODE_RESET);\
    (lexer).print_error_message_line(location);\
    eprintln("");\
    std::exit(1);\
} while(0)

using Token_Pool = Pool<Token, 1024>;

struct Lexer {
    std::unordered_map<uint32_t, std::string_view> line_map = {};
    std::string buffer_of_source_code = {}; // Fuck std::string::substr, it returns a std::string instead of std::string_view.
    Token_Pool token_pool = {};
    std::string_view source = {}; // A view into the buffer for the source code.
    std::string_view filename = {};
    Location location = { .line = 1, .column = 0 };
    size_t cursor = 0;
    size_t current_line_start = 0;

    Lexer(std::string_view path)
        : filename(path)
    {
        buffer_of_source_code = read_entire_file(path);
        source = buffer_of_source_code;
    }

    constexpr Option<char> peek_next() const
    {
        if (cursor + 1 < source.size()) {
            return Some(source[cursor + 1]);
        }
        return None(char);
    }

    constexpr bool is_eof() const
    {
        return cursor >= source.size();
    }

    constexpr const Token &get_token(Token_ID id) const { return token_pool.get(id); }
    constexpr       Token &get_token(Token_ID id)       { return token_pool.get(id); }

    Token_ID push_token(Token_Kind kind, Location loc);
    Token_ID push_token(Token_Kind kind, Location loc, Token_Data data);
    void advance(size_t count);
    void skip_whitespace();
    void print_error_message_line(Location error_location);
    void print_token_stream();
    void lex();
    void lex_identifier();
    void lex_number_literal();
    void lex_char_literal();
    void lex_string_literal();
    void lex_slash();
};

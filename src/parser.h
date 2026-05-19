#pragma once

#include "lexer.h"
#include "core/dynamic_array.h"
#include <unordered_map>
#include <string>


// Expressions
// -------------------------------------------------------- //

enum class Number_Kind { None, Integer, Float, Char };

struct Expr_Number {
    union { uint64_t uint_value; double float_value; char char_value; };
    Number_Kind kind;
};

enum class Unary_Expr_Kind {
    Plus,
    Minus,
};

enum class Binary_Expr_Kind {
    None,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    LogicalOr,
    LogicalAnd,
    BitwiseOr,
    BitwiseAnd,
    BitwiseXor,
};

enum class Expr_Kind {
    None,
    Number,
    Tuple,
    Unary,
    Binary,
    Grouping,
};

struct Expr {
    union {
        Expr_Number number;
        struct { Dynamic_Array<Expr*> expressions; } tuple;
        struct { Expr *p_expr; } grouping;
        struct { Expr *p_expr; Unary_Expr_Kind kind; } unary;
        struct { Expr *p_left, *p_right; Binary_Expr_Kind kind; } binary;
    };
    Location location;
    Expr_Kind kind;
};

// Types
// -------------------------------------------------------- //

enum class Type_Annotation_Kind {
    None,
    Array,
    Builtin, // It includes primitives, strings and math types.
    Pointer,
    Slice,
    Tuple,
    UserDefined, // Structs, unions, enums defined by the program.
};

struct Type_Annotation {
    union {
        struct { const Type_Annotation *p_annotation; size_t count; } array; // examples: [2]f32, [4]string
        struct { Token_Kind keyword; } builtin; // examples: i32, mat4, string
        struct { const Type_Annotation *p_annotation; } pointer; // examples: *u8, *void, *Entity
        struct { const Type_Annotation *p_annotation; } slice; // examples: []string, []u8, []i32
        struct { Dynamic_Array<Type_Annotation> types; } tuple; // examples: (i32, string), (*void, []u8, bool)
        struct { String_View name; } user_defined; // examples: Entity, Player
    };
    Location location;
    Type_Annotation_Kind kind;
};

struct Typed_Identifier_Group {
    Type_Annotation type_annotation;
    Dynamic_Array<const Token*> identifiers;
};

// Statements
// -------------------------------------------------------- //

// Struct, Union, Enum, and Function definitions
// are not considered statements because they are
// only allowed at file scope. 
//
// Statements are only allowed inside the body of functions.
// Out of all the statements, only the variable definitions
// are allowed both at file scope and at function bodies.
//
// Non constant expressions are not allowed in the variable
// initializers on file scope though.
enum class Stmt_Kind {
    None,
    Break,
    Continue,
    Expr,
    Loop,
    Scope,
    VariableDefinition, // let and const
};

struct Variable_Definition {
    Option<Type_Annotation> type_annotation;
    String_View name;
    Expr *p_initializer; // nullptr indicates no initializer.
    bool is_const;
};

struct Stmt;
struct Stmt_Loop {
    const Token* p_label; // nullptr indicates no label.
    Option<Variable_Definition> before;
    Expr *p_condition; // nullptr indicates no condition expression.
    Expr *p_after;     // nullptr indicates no after expression.
    Dynamic_Array<Stmt> body;
};

struct Stmt {
    union {
        struct { const Token *p_label; /*nullptr indicates no label*/ } _break;
        struct { const Token *p_label; /*nullptr indicates no label*/ } _continue;
        Expr *p_expr;
        Stmt_Loop loop;
        Dynamic_Array<Stmt> scope_block;
        Variable_Definition variable_definition;
    };
    Location location;
    Stmt_Kind kind;
};

// Functions
// -------------------------------------------------------- //

struct Function_Signature {
    Type_Annotation return_type;
    Dynamic_Array<Typed_Identifier_Group> args;
    String_View name;
    Location location;
};

struct Function_Definition {
    Function_Signature signature;
    Dynamic_Array<Stmt> statements;
};

// Structs, Unions, Enums
// -------------------------------------------------------- //
 
// All structs must have a name. No anonymous structs.
struct Struct_Definition {
    Dynamic_Array<Typed_Identifier_Group> fields;
    String_View name;
    Location location;
};

// All unions must have a name. No anonymous unions.
struct Union_Definition {
    Dynamic_Array<Typed_Identifier_Group> fields;
    String_View name;
    Location location;
};

struct Enum_Listing {
    String_View name;
    Location location;
    uint64_t value;
};

// All enums must have a name. No anonymous enums.
struct Enum_Definition {
    Dynamic_Array<Enum_Listing> listings;
    String_View name;
    Location location;
};

// Precedence Levels
// -------------------------------------------------------- //

// Almost the same as the C precedence table, but with some operators missing.
enum class Precedence {
    None,
    // Precedence   Associativity   Operators
    Assignment,  // right-to-left   = += -= *= /= %= >>= <<= &= ^= |=
    LogicalOr,   // left-to-right   ||
    LogicalAnd,  // left-to-right   &&
    BitwiseOr,   // left-to-right   |
    BitwiseXor,  // left-to-right   ^
    BitwiseAnd,  // left-to-right   &
    Equality,    // left-to-right   == !=
    Comparison,  // left-to-right   < > <= >=
    Bitshift,    // left-to-right   << >>
    AddSub,      // left-to-right   + -
    MulDivMod,   // left-to-right   * / %
    Level2,      // right-to-left   ++prefix --prefix +unary -unary ! ~ cast(type) *dereference &addressof
    Level1,      // left-to-right   () [] . {} postfix++ postfix--
};


#define get_next_level(precedence) ((Precedence)((int)(precedence) + 1))

struct Parser;

// Prefix parsing functions take no arguments and return an Expression pointer.
using Parse_Prefix_Fn = Expr *(Parser::*)();

// Infix parsing functions take the left operand and return an Expression pointer.
using Parse_Infix_Fn = Expr *(Parser::*)(Expr *p_left);

struct Parse_Rule {
    Parse_Prefix_Fn prefix;
    Parse_Infix_Fn infix;
    Precedence precedence;
};

Parse_Rule get_parse_rule(Token_Kind kind);

// The Parser
// -------------------------------------------------------- //

struct Parser {
    std::unordered_map<std::string, Function_Definition> function_definitions = {};
    std::unordered_map<std::string, Struct_Definition> struct_definitions = {};
    std::unordered_map<std::string, Union_Definition> union_definitions = {};
    std::unordered_map<std::string, Enum_Definition> enum_definitions = {};
    std::unordered_map<std::string, Variable_Definition> global_variable_definitions = {};
    Pool<Type_Annotation, 256> type_annotation_pool = {};
    Pool<Expr, 1024> expression_pool = {};
    Token_Pool::Iterator cursor; // Token Pool iterator that is the current token.
    const Token *p_current_token = nullptr; // Has to be in sync with the cursor.
    const Token *p_previous_token = nullptr; // Has to be in sync with the token previous to the cursor.
    Lexer *p_lexer = nullptr; // Pointer to the lexer that has all the tokens.

    Parser(Lexer *lexer)
        : cursor{lexer->token_pool.begin()}
        , p_lexer{lexer}
    {
        debug_assert(cursor.id.block_node != nullptr && "The lexer must already have tokens.");
        p_current_token = lexer->token_pool.get_ptr(cursor.id);
    }

    constexpr Token peek() const
    {
        return *p_current_token;
    }

    constexpr Token peek_prev() const
    {
        return *p_previous_token;
    }

    constexpr bool is_eof() const
    {
        return peek().kind == Token_Kind::EndOfFile;
    }

    void advance(size_t count = 1);
    const Token *consume(Token_Kind expected);
    void parse();
    void parse_fn_def();
    void parse_struct_def();
    void parse_union_def();
    void parse_enum_def();
    Variable_Definition parse_variable_definition();
    Stmt parse_stmt();
    Stmt parse_variable_definition_stmt();
    Stmt parse_scope_block_stmt();
    Stmt parse_labeled_loop_stmt();
    Stmt parse_for_stmt(const Token *p_label = nullptr);
    Stmt parse_while_stmt(const Token *p_label = nullptr);
    Stmt parse_loop_stmt(const Token *p_label = nullptr);
    Stmt parse_break_stmt();
    Stmt parse_continue_stmt();
    Stmt parse_expr_stmt();
    Dynamic_Array<Stmt> parse_scope_block();
    Typed_Identifier_Group parse_typed_identifier_group();
    Type_Annotation parse_type_annotation();
    Function_Signature parse_function_signature();
    Expr *parse_expr();
    Expr *parse_precedence(Precedence precedence);
    Expr *parse_number();
    Expr *parse_tuple_or_grouping();
    Expr *parse_unary();
    Expr *parse_binary(Expr *p_left);
};

#pragma once

#include "lexer.h"
#include "core/dynamic_array.h"
#include <unordered_map>
#include <string>


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

void print_type_annotation(const Type_Annotation &annotation);

struct Typed_Identifier_Group {
    Type_Annotation type_annotation;
    Dynamic_Array<const Token*> identifiers;
};

// Expressions
// -------------------------------------------------------- //

enum class Number_Kind { None, Integer, Float, Char };

struct Expr_Number {
    union { uint64_t uint_value; double float_value; char char_value; };
    Number_Kind kind;
};

template <>
struct std::formatter<Expr_Number> {
    constexpr auto parse(std::format_parse_context &ctx) {
        return ctx.begin();
    }
    auto format(const Expr_Number &number, std::format_context &ctx) const {
        switch (number.kind) {
        case Number_Kind::Integer:
            return std::format_to(ctx.out(), "{}", number.uint_value);
        case Number_Kind::Float:
            return std::format_to(ctx.out(), "{}", number.float_value);
        case Number_Kind::Char:
            return std::format_to(ctx.out(), "'{}'", escape_char(number.char_value));
        }
        unreachable();
    }
};

// NOTE: If you modify this, please also modify cstring_from(Unary_Expr_Kind)
enum class Unary_Expr_Kind {
    None,
    Plus,
    Minus,
    Dereference,
    Addressof,
    LogicalNot,
    BitwiseNot,
};

const char *cstring_from(Unary_Expr_Kind kind);

// NOTE: If you modify this, please also modify cstring_from(Binary_Expr_Kind)
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
    Equal,
    NotEqual,
    GreaterThan,
    LessThan,
    GreaterEqual,
    LessEqual,
    ShiftLeft,
    ShiftRight,
};

const char *cstring_from(Binary_Expr_Kind kind);

// NOTE: If you modify this, please also modify cstring_from(Assignment_Kind)
enum class Assignment_Kind {
    None,
    Equal,
    AddEqual,
    SubEqual,
    MulEqual,
    DivEqual,
    ModEqual,
    ShiftLeftEqual,
    ShiftRightEqual,
    LogicalAndEqual,
    LogicalOrEqual,
    BitwiseAndEqual,
    BitwiseXorEqual,
    BitwiseOrEqual,
};

const char *cstring_from(Assignment_Kind kind);

enum class Initializer_List_Kind {
    None,
    Named, // Used for structs and unions. Field names are prefixed with a '.'
    Unnamed, // Used for arrays.
};

struct Expr;

struct Initializer_List {
    struct Named_Field { const Token *p_name; Expr *p_expr; };
    union {
        struct { Dynamic_Array<Named_Field> fields; } named;
        struct { Dynamic_Array<Expr*> expressions; } unnamed;
    };
    Type_Annotation type_annotation;
    Initializer_List_Kind kind;
};

// NOTE: If you modify this, please also modify print_expr()
enum class Expr_Kind {
    None,
    Number,
    StringLiteral,
    Tuple,
    Cast,
    Unary,
    Binary,
    Assignment,
    Grouping,
    FunctionCall,
    ArraySubscript,
    FieldAccess,
    Variable,
    InitializerList,
    True,  // Does not use any data in the expression union variants.
    False, // Does not use any data in the expression union variants.
    Null,  // Does not use any data in the expression union variants.
};

struct Expr {
    union {
        Expr_Number number;
        String_View string_literal;
        struct { Dynamic_Array<Expr*> expressions; } tuple;
        struct { Type_Annotation type_annotation; Expr *p_expr; } cast;
        struct { Expr *p_expr; Unary_Expr_Kind kind; } unary;
        struct { Expr *p_left, *p_right; Binary_Expr_Kind kind; } binary;
        struct { Expr *p_left, *p_right; Assignment_Kind kind; } assignment;
        struct { Expr *p_expr; } grouping;
        struct { Expr *p_left; Dynamic_Array<Expr *> arguments; } function_call;
        struct { Expr *p_left; Expr *p_index; } array_subscript;
        struct { Expr *p_left; String_View field_name; } field_access;
        struct { Type_Annotation type_annotation; String_View name; } variable;
        Initializer_List initializer_list;
    };
    Location location;
    Expr_Kind kind;
};

void print_expr(const Expr *p_expr, size_t level = 0);

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
//
// NOTE: If you modify this, please also modify print_stmt()
enum class Stmt_Kind {
    None,
    Break,
    Continue,
    Return,
    If,
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

void print_variable_definition(const Variable_Definition &var_def);

struct Stmt;
struct If_Branch {
    Expr *p_condition;
    Dynamic_Array<Stmt> statements;
};

struct Stmt_If {
    If_Branch if_branch;
    Dynamic_Array<If_Branch> else_if_branches;
    Dynamic_Array<Stmt> else_statements; // Should be empty if there's no else branch.
};

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
        struct { Expr *p_expr; } _return;
        Stmt_If _if;
        Expr *p_expr;
        Stmt_Loop loop;
        struct { Dynamic_Array<Stmt> statements; } scope_block;
        Variable_Definition variable_definition;
    };
    Location location;
    Stmt_Kind kind;
};

void println_stmt(const Stmt &stmt, size_t level);


// Functions
// -------------------------------------------------------- //

struct Function_Signature {
    Type_Annotation return_type;
    Dynamic_Array<Typed_Identifier_Group> args;
    String_View name;
    Location location;
};

void print_function_signature(const Function_Signature &signature);

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
    Assignment,  // right-to-left   = += -= *= /= %= >>= <<= &&= ||= &= ^= |=
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
    Level2,      // right-to-left   +unary -unary ! ~ cast(type) *dereference &addressof
    Level1,      // left-to-right   () [] . {}
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

    constexpr Token peek_next() const
    {
        return *cursor.get_next();
    }

    constexpr bool is_eof() const
    {
        return peek().kind == Token_Kind::EndOfFile;
    }

    void print_results();
    void advance(size_t count = 1);
    const Token *consume(Token_Kind expected, const char *message);
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
    Stmt parse_return_stmt();
    Stmt parse_if_stmt();
    Stmt parse_expr_stmt();
    Dynamic_Array<Stmt> parse_scope_block();
    Typed_Identifier_Group parse_typed_identifier_group();
    Type_Annotation parse_type_annotation();
    Function_Signature parse_function_signature();
    Expr *parse_expr();
    Expr *parse_precedence(Precedence precedence);
    Expr *parse_literal();
    Expr *parse_number();
    Expr *parse_string_lit();
    Expr *parse_initializer_list();
    Expr *parse_variable();
    Expr *parse_tuple_or_grouping();
    Expr *parse_cast();
    Expr *parse_unary();
    Expr *parse_binary(Expr *p_left);
    Expr *parse_assignment(Expr *p_left);
    Expr *parse_call(Expr *p_left);
    Expr *parse_array_subscript(Expr *p_left);
    Expr *parse_dot(Expr *p_left);
};

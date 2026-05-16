#pragma once

#include "lexer.h"
#include "core/dynamic_array.h"
#include <unordered_map>
#include <variant>
#include <string>


// Expressions
// -------------------------------------------------------- //

enum class Expr_Kind {
    None,
    // TODO: Fill this in.
};

struct Expr {
    // TODO: Fill this in.
    Expr_Kind kind;
};

// Types
// -------------------------------------------------------- //

enum class Type_Annotation_Kind {
    None,
    UserDefined, // Structs, unions, enums defined by the program.
    Builtin, // It includes primitives, strings and math types.
    Array,
    Slice,
    Pointer,
};

struct Type_Annotation {
    union {
        struct { String_View name; } user_defined; // examples: Entity, Player
        struct { Token_Kind keyword; } builtin; // examples: i32, mat4, string
        struct { Type_Annotation *annotation; size_t count; } array; // examples: [2]f32, [4]string
        struct { Type_Annotation *annotation; } slice; // examples: []string, []u8, []i32
        struct { Type_Annotation *annotation; } pointer; // examples: *u8, *void, *Entity
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
    Option<Expr> initializer;
    bool is_const;
};

struct Stmt_Break {
    Option<String_View> label;
};

struct Stmt_Continue {
    Option<String_View> label;
};

struct Stmt;
struct Stmt_Loop {
    Option<String_View> label;
    Option<Variable_Definition> initializer;
    Option<Expr> condition;
    Option<Expr> after;
    Dynamic_Array<Stmt> body;
};

using Stmt_Variant = std::variant<
    Stmt_Break,
    Stmt_Continue,
    Expr,
    Stmt_Loop,
    Dynamic_Array<Stmt>,
    Variable_Definition>;

struct Stmt {
    Stmt_Variant variant;
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

// The Parser
// -------------------------------------------------------- //

struct Parser {
    std::unordered_map<std::string, Function_Definition> function_definitions = {};
    std::unordered_map<std::string, Struct_Definition> struct_definitions = {};
    std::unordered_map<std::string, Union_Definition> union_definitions = {};
    std::unordered_map<std::string, Enum_Definition> enum_definitions = {};
    std::unordered_map<std::string, Variable_Definition> global_variable_definitions = {};
    Token_Pool::Iterator cursor; // Token Pool iterator that is the current token.
    const Token *p_current_token = nullptr; // Has to be in sync with the cursor.
    Lexer *p_lexer = nullptr; // Pointer to the lexer that has all the tokens.

    Parser(Lexer *lexer)
        : cursor{lexer->token_pool.begin()}
        , p_lexer{lexer}
    {
        Assert(cursor.id.block_node != nullptr && "The lexer must already have tokens.");
        p_current_token = lexer->token_pool.get_ptr(cursor.id);
    }

    constexpr Token peek() const
    {
        return *p_current_token;
    }

    constexpr bool is_eof() const
    {
        return peek().kind == Token_Kind::EndOfFile;
    }

    void advance(size_t count = 1);
    Token consume(Token_Kind expected);
    void parse();
    void parse_fn_def();
    void parse_struct_def();
    void parse_union_def();
    void parse_enum_def();
    Stmt parse_stmt();
    Stmt parse_variable_definition_stmt(bool is_const);
    Stmt parse_scope_block_stmt();
    Stmt parse_labeled_loop_stmt();
    Stmt parse_for_stmt(const Token *p_label = nullptr);
    Stmt parse_while_stmt(const Token *p_label = nullptr);
    Stmt parse_break_stmt();
    Stmt parse_continue_stmt();
    Stmt parse_expr_stmt();
    Dynamic_Array<Stmt> parse_scope_block();
};

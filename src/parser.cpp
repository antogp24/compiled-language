#include "parser.h"

void Parser::advance(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        Assert(!is_eof());
        ++cursor; // Go to the next token (using the iterator's overloaded ++operator).
        p_current_token = p_lexer->token_pool.get_ptr(cursor.id);
    }
}

Token Parser::consume(Token_Kind expected)
{
    Token consumed = peek();
    if (consumed.kind != expected) {
        error_at(*p_lexer, consumed.location, "Expected {}, but got {}",
            magic_enum::enum_name(expected), magic_enum::enum_name(consumed.kind));
    }
    advance();
    return consumed;
}

void Parser::parse()
{
    while (!is_eof()) {
        switch (peek().kind) {
        case Token_Kind::Fn: parse_fn_def(); break;
        case Token_Kind::Struct: parse_struct_def(); break;
        case Token_Kind::Union: parse_union_def(); break;
        case Token_Kind::Enum: parse_enum_def(); break;
        case Token_Kind::Let: {
            Stmt stmt = parse_variable_definition_stmt(false);
            Variable_Definition var = std::get<Variable_Definition>(stmt.variant);
            global_variable_definitions[var.name.to_std_string()] = var;
        }break;
        case Token_Kind::Const: {
            Stmt stmt = parse_variable_definition_stmt(true);
            Variable_Definition var = std::get<Variable_Definition>(stmt.variant);
            global_variable_definitions[var.name.to_std_string()] = var;
        }break;
        default:
            error_at(*p_lexer, peek().location,
                "Expected a global variable, function, struct, union, or enum definition.");
        }
    }
}

void Parser::parse_fn_def()
{
    feature_todo(*p_lexer, peek().location, "Parsing function definitions.");
}

void Parser::parse_struct_def()
{
    feature_todo(*p_lexer, peek().location, "Parsing struct definitions.");
}

void Parser::parse_union_def()
{
    feature_todo(*p_lexer, peek().location, "Parsing union definitions.");
}

void Parser::parse_enum_def()
{
    feature_todo(*p_lexer, peek().location, "Parsing enum definitions.");
}

Stmt Parser::parse_stmt()
{
    switch (peek().kind) {
    case Token_Kind::Let: return parse_variable_definition_stmt(false);
    case Token_Kind::Const: return parse_variable_definition_stmt(true);
    case Token_Kind::BraceLeft: return parse_scope_block_stmt();
    case Token_Kind::HashQuote: return parse_labeled_loop_stmt();
    case Token_Kind::For: return parse_for_stmt();
    case Token_Kind::While: return parse_while_stmt();
    case Token_Kind::Break: return parse_break_stmt();
    case Token_Kind::Continue: return parse_continue_stmt();
    case Token_Kind::EndOfFile: unreachable(); break;
    }
    return parse_expr_stmt();
}

Stmt Parser::parse_variable_definition_stmt(bool is_const)
{
    (void)is_const;
    feature_todo(*p_lexer, peek().location, "Parsing variable definitions.");
}

Stmt Parser::parse_scope_block_stmt()
{
    Dynamic_Array<Stmt> stmts = parse_scope_block();
    return Stmt{ .variant = stmts, .kind = Stmt_Kind::Scope };
}

Stmt Parser::parse_labeled_loop_stmt()
{
    feature_todo(*p_lexer, peek().location, "Parsing labeled loops.");
}

Stmt Parser::parse_for_stmt(const Token *p_label)
{
    (void)p_label;
    feature_todo(*p_lexer, peek().location, "Parsing for loops.");
}

Stmt Parser::parse_while_stmt(const Token *p_label)
{
    (void)p_label;
    feature_todo(*p_lexer, peek().location, "Parsing while loops.");
}

Stmt Parser::parse_break_stmt()
{
    feature_todo(*p_lexer, peek().location, "Parsing break statements.");
}

Stmt Parser::parse_continue_stmt()
{
    feature_todo(*p_lexer, peek().location, "Parsing continue statements.");
}

Stmt Parser::parse_expr_stmt()
{
    feature_todo(*p_lexer, peek().location, "Parsing expression statements.");
}

Dynamic_Array<Stmt> Parser::parse_scope_block()
{
    feature_todo(*p_lexer, peek().location, "Parsing scope blocks.");
}

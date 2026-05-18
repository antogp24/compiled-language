#include "parser.h"

void Parser::advance(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        debug_assert(!is_eof());
        ++cursor; // Go to the next token (using the iterator's overloaded ++operator).
        p_current_token = p_lexer->token_pool.get_ptr(cursor.id);
    }
}

const Token *Parser::consume(Token_Kind expected)
{
    const Token *consumed = p_current_token;
    if (consumed->kind != expected) {
        error_at(*p_lexer, consumed->location, "Expected {}, but got {}",
            magic_enum::enum_name(expected), magic_enum::enum_name(consumed->kind));
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

        case Token_Kind::Let:
        case Token_Kind::Const:
        {
            Variable_Definition var = parse_variable_definition();
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
    Function_Signature signature = parse_function_signature();
    Dynamic_Array<Stmt> statements = parse_scope_block();
    function_definitions[signature.name.to_std_string()] = Function_Definition{
        .signature = signature,
        .statements = statements,
    };
}

void Parser::parse_struct_def()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Struct);
    advance();

    String_View name = consume(Token_Kind::Identifier)->data.str;
    consume(Token_Kind::BraceLeft);
    Dynamic_Array<Typed_Identifier_Group> fields = {};

    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Typed_Identifier_Group field = parse_typed_identifier_group();
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma);
        } else if (peek().kind == Token_Kind::Comma) {
            advance(); // trailing comma is optional.
        }
        fields.append(field);
    }
    consume(Token_Kind::BraceRight);

    struct_definitions[name.to_std_string()] = Struct_Definition{
        .fields = fields,
        .name = name,
        .location = loc,
    };
}

void Parser::parse_union_def()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Union);
    advance();

    String_View name = consume(Token_Kind::Identifier)->data.str;
    consume(Token_Kind::BraceLeft);
    Dynamic_Array<Typed_Identifier_Group> fields = {};

    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Typed_Identifier_Group field = parse_typed_identifier_group();
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma);
        } else if (peek().kind == Token_Kind::Comma) {
            advance(); // trailing comma is optional.
        }
        fields.append(field);
    }
    consume(Token_Kind::BraceRight);

    union_definitions[name.to_std_string()] = Union_Definition{
        .fields = fields,
        .name = name,
        .location = loc,
    };
}

void Parser::parse_enum_def()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Enum);
    advance();

    String_View name = consume(Token_Kind::Identifier)->data.str;
    consume(Token_Kind::BraceLeft);

    Dynamic_Array<Enum_Listing> listings = {};
    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        const Token *identifier = consume(Token_Kind::Identifier);
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma);
        }
        Enum_Listing listing = {
            .name = identifier->data.str,
            .location = identifier->location,
            .value = listings.count,
        };
        listings.append(listing);
    }
    consume(Token_Kind::BraceRight);

    enum_definitions[name.to_std_string()] = Enum_Definition{
        .listings = listings,
        .name = name,
        .location = loc,
    };
}

Variable_Definition Parser::parse_variable_definition()
{
    debug_assert(peek().kind == Token_Kind::Let || peek().kind == Token_Kind::Const);
    bool is_const = peek().kind == Token_Kind::Const;
    advance();

    String_View name = consume(Token_Kind::Identifier)->data.str;

    Option<Type_Annotation> type_annotation = None(Type_Annotation);
    if (peek().kind == Token_Kind::Colon) {
        advance();
        type_annotation = Some(parse_type_annotation());
    }

    Option<Expr> initializer = None(Expr);
    if (peek().kind == Token_Kind::Equal) {
        advance();
        initializer = Some(parse_expr());
    }
    if (is_const && initializer.is_none()) {
        error_at(*p_lexer, peek().location, "An initializer is necessary for a constant, but got none.");
    }

    consume(Token_Kind::Semicolon);

    return Variable_Definition{ type_annotation, name, initializer, is_const };
}

// Statements are only allowed inside functions.
Stmt Parser::parse_stmt()
{
    switch (peek().kind) {
    case Token_Kind::Let: return parse_variable_definition_stmt();
    case Token_Kind::Const: return parse_variable_definition_stmt();
    case Token_Kind::BraceLeft: return parse_scope_block_stmt();
    case Token_Kind::HashQuote: return parse_labeled_loop_stmt();
    case Token_Kind::For: return parse_for_stmt();
    case Token_Kind::While: return parse_while_stmt();
    case Token_Kind::Loop: return parse_loop_stmt();
    case Token_Kind::Break: return parse_break_stmt();
    case Token_Kind::Continue: return parse_continue_stmt();
    case Token_Kind::EndOfFile: unreachable();
    case Token_Kind::Fn:
        error_at(*p_lexer, peek().location, "Function definitions are not allowed inside functions.");
    case Token_Kind::Struct:
        error_at(*p_lexer, peek().location, "Struct definitions are not allowed inside functions.");
    case Token_Kind::Union:
        error_at(*p_lexer, peek().location, "Union definitions are not allowed inside functions.");
    case Token_Kind::Enum: 
        error_at(*p_lexer, peek().location, "Enum definitions are not allowed inside functions.");
    }
    return parse_expr_stmt();
}

Stmt Parser::parse_variable_definition_stmt()
{
    Location loc = peek().location;
    Variable_Definition variable_definition = parse_variable_definition();
    return Stmt{
        .variable_definition = variable_definition,
        .location = loc,
        .kind = Stmt_Kind::VariableDefinition,
    };
}

Stmt Parser::parse_scope_block_stmt()
{
    Dynamic_Array<Stmt> stmts = parse_scope_block();
    return Stmt{ .scope_block = stmts, .kind = Stmt_Kind::Scope };
}

Stmt Parser::parse_labeled_loop_stmt()
{
    debug_assert(peek().kind == Token_Kind::HashQuote);
    advance();
    const Token *p_label = consume(Token_Kind::Identifier);
    consume(Token_Kind::Colon);

    switch (peek().kind) {
    case Token_Kind::For: return parse_for_stmt(p_label);
    case Token_Kind::While: return parse_while_stmt(p_label);
    case Token_Kind::Loop: return parse_loop_stmt(p_label);
    }
    error_at(*p_lexer, peek().location,
        "Expected one of the looping statements (for, while, loop), but got {}",
        magic_enum::enum_name(peek().kind));
}

Stmt Parser::parse_for_stmt(const Token *p_label)
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::For);
    advance();

    Option<Variable_Definition> initializer = {};
    if (peek().kind == Token_Kind::Let) {
        initializer = Some(parse_variable_definition()); // already consumes the semicolon.
    } else {
        consume(Token_Kind::Semicolon);
    }

    Option<Expr> condition = {};
    if (peek().kind != Token_Kind::Semicolon) {
        condition = Some(parse_expr());
    }
    consume(Token_Kind::Semicolon);

    Option<Expr> after = {};
    if (peek().kind != Token_Kind::Semicolon) {
        condition = Some(parse_expr());
    }
    consume(Token_Kind::Semicolon);

    Dynamic_Array<Stmt> body = parse_scope_block();

    return Stmt{
        .loop = {
            .p_label = p_label,
            .initializer = initializer,
            .condition = condition,
            .after = after,
            .body = body,
        },
        .location = loc,
        .kind = Stmt_Kind::Loop,
    };
}

Stmt Parser::parse_while_stmt(const Token *p_label)
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::While);
    advance();

    Option<Expr> condition = Some(parse_expr());
    Dynamic_Array<Stmt> body = parse_scope_block();

    return Stmt{
        .loop = {
            .p_label = p_label,
            .condition = condition,
            .body = body,
        },
        .location = loc,
        .kind = Stmt_Kind::Loop,
    };
}

Stmt Parser::parse_loop_stmt(const Token *p_label)
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Loop);
    advance();

    Dynamic_Array<Stmt> body = parse_scope_block();

    return Stmt{
        .loop = {
            .p_label = p_label,
            .body = body,
        },
        .location = loc,
        .kind = Stmt_Kind::Loop,
    };
}

Stmt Parser::parse_break_stmt()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Break);
    advance();

    const Token *p_label = nullptr;
    if (peek().kind == Token_Kind::Identifier) {
        p_label = p_current_token;
        advance();
    }
    consume(Token_Kind::Semicolon);

    return Stmt{ ._break = { .p_label = p_label }, .location = loc, .kind = Stmt_Kind::Break };
}

Stmt Parser::parse_continue_stmt()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Continue);
    advance();

    const Token *p_label = nullptr;
    if (peek().kind == Token_Kind::Identifier) {
        p_label = p_current_token;
        advance();
    }
    consume(Token_Kind::Semicolon);

    return Stmt{ ._continue = { .p_label = p_label }, .location = loc, .kind = Stmt_Kind::Continue };
}

Stmt Parser::parse_expr_stmt()
{
    Location loc = peek().location;
    Expr expr = parse_expr();
    consume(Token_Kind::Semicolon);
    return Stmt{ .expr = expr, .location = loc, .kind = Stmt_Kind::Expr };
}

Dynamic_Array<Stmt> Parser::parse_scope_block()
{
    consume(Token_Kind::BraceLeft);
    Dynamic_Array<Stmt> statements = {};
    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Stmt stmt = parse_stmt();
        statements.append(stmt);
    }
    consume(Token_Kind::BraceRight);

    return statements;
}

Typed_Identifier_Group Parser::parse_typed_identifier_group()
{
    Dynamic_Array<const Token *> identifiers = {};
    const Token *first = consume(Token_Kind::Identifier);
    identifiers.append(first);

    while (!is_eof() && peek().kind == Token_Kind::Comma) {
        advance(); // consume the comma.
        const Token *identifier = consume(Token_Kind::Identifier);
        identifiers.append(identifier);
    }
    consume(Token_Kind::Colon);
    Type_Annotation type_annotation = parse_type_annotation();

    return Typed_Identifier_Group{
        .type_annotation = type_annotation,
        .identifiers = identifiers,
    };
}
Type_Annotation Parser::parse_type_annotation()
{
    Location loc = peek().location;
    if (is_builtin(peek().kind)) {
        Token_Kind keyword = peek().kind;
        advance();
        return Type_Annotation{
            .builtin = { .keyword = keyword },
            .location = loc,
            .kind = Type_Annotation_Kind::Builtin,
        };
    }

    switch (peek().kind) {
    case Token_Kind::ParenLeft: {
        advance();
        Dynamic_Array<Type_Annotation> types = {};
        while (!is_eof() && peek().kind != Token_Kind::ParenRight) {
            Type_Annotation type_annotation = parse_type_annotation();
            if (!is_eof() && peek().kind != Token_Kind::ParenRight) {
                consume(Token_Kind::Comma);
            }
            types.append(type_annotation);
        }
        consume(Token_Kind::ParenRight);
        return Type_Annotation{
            .tuple = { .types = types },
            .location = loc,
            .kind = Type_Annotation_Kind::Tuple,
        };
    } break;
    case Token_Kind::BracketLeft: {
        advance();
        switch (peek().kind) {
        case Token_Kind::Int_Literal: {
            advance();
            consume(Token_Kind::BracketRight);
            Type_Annotation *p_type_annotation = type_annotation_pool.append();
            *p_type_annotation = parse_type_annotation();
            return Type_Annotation{
                .array = { .p_annotation = p_type_annotation },
                .location = loc,
                .kind = Type_Annotation_Kind::Array,
            };
        } break;
        case Token_Kind::BracketRight: {
            advance();
            Type_Annotation *p_type_annotation = type_annotation_pool.append();
            *p_type_annotation = parse_type_annotation();
            return Type_Annotation{
                .slice = { .p_annotation = p_type_annotation},
                .location = loc,
                .kind = Type_Annotation_Kind::Slice,
            };
        } break;
        default:
            // TODO: Support arrays that inside of the square brackets have identifiers
            // that represent constants.
            error_at(*p_lexer, peek().location,
                "Expected a closing square bracket or a positive number, "
                "to determine if the type is a slice or an array, but got {}",
                magic_enum::enum_name(peek().kind));
        }
    } break;
    case Token_Kind::Star: {
        advance();
        Type_Annotation *p_type_annotation = type_annotation_pool.append();
        *p_type_annotation = parse_type_annotation();
        return Type_Annotation{
            .pointer = { .p_annotation = p_type_annotation },
            .location = loc,
            .kind = Type_Annotation_Kind::Pointer,
        };
    } break;
    case Token_Kind::Identifier: {
        String_View name = peek().data.str;
        advance();
        return Type_Annotation{
            .user_defined = { .name = name },
            .location = loc,
            .kind = Type_Annotation_Kind::UserDefined,
        };
    } break;
    }
    error_at(*p_lexer, peek().location, "Invalid type annotation");
}

Function_Signature Parser::parse_function_signature()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Fn);
    advance();

    String_View name = consume(Token_Kind::Identifier)->data.str;
    consume(Token_Kind::ParenLeft);

    Dynamic_Array<Typed_Identifier_Group> args = {};
    while (!is_eof() && peek().kind == Token_Kind::Identifier) {
        Typed_Identifier_Group arg = parse_typed_identifier_group();
        if (peek().kind == Token_Kind::Comma) {
            advance();
        }
        args.append(arg);
    }
    consume(Token_Kind::ParenRight);

    Type_Annotation return_type;
    if (peek().kind != Token_Kind::BraceLeft) {
        consume(Token_Kind::Arrow);
        return_type = parse_type_annotation();
    } else {
        return_type = Type_Annotation{
            .builtin = { .keyword = Token_Kind::Void },
            .location = peek().location,
            .kind = Type_Annotation_Kind::Builtin,
        };
    }

    return Function_Signature{
        .return_type = return_type,
        .args = args,
        .name = name,
        .location = loc,
    };
}

Expr Parser::parse_expr()
{
    feature_todo(*p_lexer, peek().location, "Parsing expressions");
}

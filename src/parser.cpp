#include "parser.h"

Parse_Rule get_parse_rule(Token_Kind kind)
{
#define case_prefix(token_kind, fn, prec)\
    case Token_Kind::token_kind:\
        return Parse_Rule{ .prefix = &Parser::fn, .infix = nullptr, .precedence = Precedence::prec }

#define case_infix(token_kind, fn, prec)\
    case Token_Kind::token_kind:\
        return Parse_Rule{ .prefix = nullptr, .infix = &Parser::fn, .precedence = Precedence::prec }

#define case_both(token_kind, prefix_fn, infix_fn, prec)\
    case Token_Kind::token_kind:\
        return Parse_Rule{ .prefix = &Parser::prefix_fn, .infix = &Parser::infix_fn, .precedence = Precedence::prec }

    switch (kind) {
        case_prefix(Int_Literal, parse_number, None);
        case_prefix(Float_Literal, parse_number, None);
        case_prefix(Char_Literal, parse_number, None);
        case_prefix(ParenLeft, parse_tuple_or_grouping, None);
        case_both(Minus, parse_unary, parse_binary, AddSub);
        case_both(Plus, parse_unary, parse_binary, AddSub);
        case_both(Star, parse_unary, parse_binary, MulDivMod);
        case_infix(Div, parse_binary, MulDivMod);
    }
    return Parse_Rule{ .prefix = nullptr, .infix = nullptr, .precedence = Precedence::None };
#undef case_none
#undef case_prefix
#undef case_infix
#undef case_both
}

void Parser::advance(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        debug_assert(!is_eof());
        p_previous_token = p_current_token;
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

    Option<Type_Annotation> type_annotation = {};
    if (peek().kind == Token_Kind::Colon) {
        advance();
        type_annotation = Some(parse_type_annotation());
    }

    Expr *p_initializer = nullptr;
    if (peek().kind == Token_Kind::Equal) {
        advance();
        p_initializer = parse_expr();
    }
    if (is_const && p_initializer == nullptr) {
        error_at(*p_lexer, peek().location, "An initializer is necessary for a constant, but got none.");
    }

    consume(Token_Kind::Semicolon);

    return Variable_Definition{ type_annotation, name, p_initializer, is_const };
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

    Option<Variable_Definition> before = {};
    if (peek().kind == Token_Kind::Let) {
        before = Some(parse_variable_definition()); // already consumes the semicolon.
    } else {
        consume(Token_Kind::Semicolon);
    }

    Expr *p_condition = nullptr;
    if (peek().kind != Token_Kind::Semicolon) {
        p_condition = parse_expr();
    }
    consume(Token_Kind::Semicolon);

    Expr *p_after = nullptr;
    if (peek().kind != Token_Kind::Semicolon) {
        p_after = parse_expr();
    }
    consume(Token_Kind::Semicolon);

    Dynamic_Array<Stmt> body = parse_scope_block();

    return Stmt{
        .loop = { p_label, before, p_condition, p_after, body },
        .location = loc,
        .kind = Stmt_Kind::Loop,
    };
}

Stmt Parser::parse_while_stmt(const Token *p_label)
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::While);
    advance();

    Expr *p_condition = parse_expr();
    Dynamic_Array<Stmt> body = parse_scope_block();

    return Stmt{
        .loop = {
            .p_label = p_label,
            .p_condition = p_condition,
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
    Expr *p_expr = parse_expr();
    consume(Token_Kind::Semicolon);
    return Stmt{ .p_expr = p_expr, .location = loc, .kind = Stmt_Kind::Expr };
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

Expr *Parser::parse_expr()
{
    return parse_precedence(get_next_level(Precedence::None));
}

Expr *Parser::parse_precedence(Precedence precedence)
{
    advance();
    Parse_Prefix_Fn prefix_rule = get_parse_rule(peek_prev().kind).prefix;
    if (prefix_rule == nullptr) {
        error_at(*p_lexer, peek_prev().location, "Expected an expression.");
    }
    Expr *p_prefix_expr = (this->*prefix_rule)();

    Precedence current_precedence = get_parse_rule(peek().kind).precedence;
    if (precedence > current_precedence) {
        return p_prefix_expr;
    }

    Expr *p_expr = nullptr;
    Expr *p_left = p_prefix_expr;

    while (!is_eof() && precedence <= (current_precedence = get_parse_rule(peek().kind).precedence))
    {
        advance();
        Parse_Infix_Fn infix_rule = get_parse_rule(peek_prev().kind).infix;
        debug_assert(infix_rule != nullptr);
        p_expr = (this->*infix_rule)(p_left);
        p_left = p_expr;
    }

    return p_expr;
}

// Assumes that it is past one the number token.
Expr *Parser::parse_number()
{
    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Number;
    p_expr->location = peek_prev().location;

    switch (peek_prev().kind) {
    case Token_Kind::Int_Literal:
        p_expr->number = {
            .uint_value = peek_prev().data.int_literal,
            .kind = Number_Kind::Integer,
        };
        break;
    case Token_Kind::Float_Literal:
        p_expr->number = {
            .float_value = peek_prev().data.float_literal,
            .kind = Number_Kind::Float,
        };
        break;
    case Token_Kind::Char_Literal:
        p_expr->number = {
            .char_value = peek_prev().data.char_literal,
            .kind = Number_Kind::Char,
        };
        break;
    default:
        unreachable();
    }

    return p_expr;
}

Expr *Parser::parse_tuple_or_grouping()
{
    debug_assert(peek_prev().kind == Token_Kind::ParenLeft);
    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;

    // Maybe this is retarded and I should make it an error.
    if (peek().kind == Token_Kind::ParenRight) {
        // It is an empty tuple: ()
        advance();
        p_expr->kind = Expr_Kind::Tuple;
        return p_expr;
    }

    Expr *p_first_expr = parse_precedence(get_next_level(Precedence::None));

    if (peek().kind == Token_Kind::Comma) {
        // It is a tuple expression.
        advance();
        p_expr->kind = Expr_Kind::Tuple;
        p_expr->tuple.expressions.append(p_first_expr);

        if (peek().kind == Token_Kind::ParenRight) {
            // It is a tuple with a single expression: (expr,)
            advance();
        } else {
            // It is a tuple with many expressions: (expr0, expr1, ...)
            consume(Token_Kind::Comma);
            while (!is_eof() && peek().kind != Token_Kind::ParenRight) {
                Expr *p_expr_inside_tuple = parse_precedence(get_next_level(Precedence::None));
                if (!is_eof() && peek().kind != Token_Kind::ParenRight) {
                    consume(Token_Kind::Comma);
                }
                p_expr->tuple.expressions.append(p_expr_inside_tuple);
            }
            consume(Token_Kind::ParenRight);
        }
    } else {
        // It is just a grouping.
        consume(Token_Kind::ParenRight);
        p_expr->kind = Expr_Kind::Grouping;
        p_expr->grouping.p_expr = p_first_expr;
    }

    return p_expr;
}

Expr *Parser::parse_unary()
{
    Token op = peek_prev();
    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Unary;
    p_expr->location = peek_prev().location;

    Expr *p_operated = parse_precedence(Precedence::Level2);

    switch (op.kind) {
    case Token_Kind::Plus:
        p_expr->unary = { .p_expr = p_operated, .kind = Unary_Expr_Kind::Plus };
        break;
    case Token_Kind::Minus:
        p_expr->unary = { .p_expr = p_operated, .kind = Unary_Expr_Kind::Minus };
        break;
    default:
        unreachable();
    }

    return p_expr;
}

Expr *Parser::parse_binary(Expr *p_left)
{
    Token op = peek_prev();
    Parse_Rule rule = get_parse_rule(op.kind);

    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Binary;
    p_expr->location = peek_prev().location;

    Expr *p_right = parse_precedence(get_next_level(rule.precedence));

    switch (op.kind) {
    case Token_Kind::Plus:
        p_expr->binary = { p_left, p_right, Binary_Expr_Kind::Add };
        break;
    case Token_Kind::Minus:
        p_expr->binary = { p_left, p_right, Binary_Expr_Kind::Sub };
        break;
    case Token_Kind::Star:
        p_expr->binary = { p_left, p_right, Binary_Expr_Kind::Mul };
        break;
    case Token_Kind::Div:
        p_expr->binary = { p_left, p_right, Binary_Expr_Kind::Div };
        break;
    default:
        unreachable();
    }

    return p_expr;
}


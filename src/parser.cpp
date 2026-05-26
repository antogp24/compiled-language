#include "parser.h"

void print_type_annotation(const Type_Annotation &annotation)
{
    switch (annotation.kind) {
    case Type_Annotation_Kind::Array:
        std::print("[{}]", annotation.array.count);
        print_type_annotation(*annotation.array.p_annotation);
        break;
    case Type_Annotation_Kind::Builtin:
        std::print("{}", magic_enum::enum_name(annotation.builtin.keyword));
        break;
    case Type_Annotation_Kind::Pointer:
        std::print("*");
        print_type_annotation(*annotation.pointer.p_annotation);
        break;
    case Type_Annotation_Kind::Slice:
        std::print("[]");
        print_type_annotation(*annotation.slice.p_annotation);
        break;
    case Type_Annotation_Kind::Tuple:
        std::print("(");
        for (size_t i = 0; i < annotation.tuple.types.count; ++i) {
            if (i > 0) {
                std::print(", ");
            }
            print_type_annotation(annotation.tuple.types[i]);
        }
        std::print(")");
        break;
    case Type_Annotation_Kind::UserDefined:
        std::print("{}", annotation.user_defined.name);
        break;
    default:
        unreachable();
    }
}

const char *cstring_from(Unary_Expr_Kind kind)
{
    switch (kind) {
    case Unary_Expr_Kind::Plus: return "+";
    case Unary_Expr_Kind::Minus: return "-";
    case Unary_Expr_Kind::Dereference: return "*";
    case Unary_Expr_Kind::Addressof: return "&";
    case Unary_Expr_Kind::LogicalNot: return "!";
    case Unary_Expr_Kind::BitwiseNot: return "~";
    }
    unreachable();
}

const char *cstring_from(Binary_Expr_Kind kind)
{
    switch (kind) {
    case Binary_Expr_Kind::Add: return "+";
    case Binary_Expr_Kind::Sub: return "-";
    case Binary_Expr_Kind::Mul: return "*";
    case Binary_Expr_Kind::Div: return "/";
    case Binary_Expr_Kind::Mod: return "%";
    case Binary_Expr_Kind::LogicalOr: return "||";
    case Binary_Expr_Kind::LogicalAnd: return "&&";
    case Binary_Expr_Kind::BitwiseOr: return "|";
    case Binary_Expr_Kind::BitwiseAnd: return "&";
    case Binary_Expr_Kind::BitwiseXor: return "^";
    case Binary_Expr_Kind::Equal: return "==";
    case Binary_Expr_Kind::NotEqual: return "!=";
    case Binary_Expr_Kind::GreaterThan: return ">";
    case Binary_Expr_Kind::LessThan: return "<";
    case Binary_Expr_Kind::GreaterEqual: return ">=";
    case Binary_Expr_Kind::LessEqual: return "<=";
    case Binary_Expr_Kind::ShiftLeft: return "<<";
    case Binary_Expr_Kind::ShiftRight: return ">>";
    }
    unreachable();
}

const char *cstring_from(Assignment_Kind kind)
{
    switch (kind) {
    case Assignment_Kind::Equal: return "=";
    case Assignment_Kind::AddEqual: return "+=";
    case Assignment_Kind::SubEqual: return "-=";
    case Assignment_Kind::MulEqual: return "*=";
    case Assignment_Kind::DivEqual: return "/=";
    case Assignment_Kind::ModEqual: return "%=";
    case Assignment_Kind::ShiftLeftEqual: return "<<=";
    case Assignment_Kind::ShiftRightEqual: return ">>=";
    case Assignment_Kind::BitwiseAndEqual: return "&=";
    case Assignment_Kind::BitwiseXorEqual: return "^=";
    case Assignment_Kind::BitwiseOrEqual: return "|=";
    }
    unreachable();
}

void print_expr(const Expr *p_expr, size_t level)
{
    debug_assert(p_expr != nullptr);

    static const char *parentheses_colors[6] = {
        ESC_CODE_RED,
        ESC_CODE_GREEN,
        ESC_CODE_YELLOW,
        ESC_CODE_BLUE,
        ESC_CODE_PURPLE,
        ESC_CODE_CYAN,
    };
    const char *color = parentheses_colors[level % 6];

    switch (p_expr->kind) {
    case Expr_Kind::Number:
        std::print("{}", p_expr->number);
        break;
    case Expr_Kind::StringLiteral:
        std::print("\"{}\"", p_expr->string_literal);
        break;
    case Expr_Kind::Tuple:
        std::print("{}(" ESC_CODE_RESET, color);
        for (size_t i = 0; i < p_expr->tuple.expressions.count; ++i) {
            if (i > 0) {
                std::print(", ");
            }
            print_expr(p_expr->tuple.expressions[i], level + 1);
        }
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::Cast:
        std::print("cast(");
        print_type_annotation(p_expr->cast.type_annotation);
        std::print(")");
        std::print("{}(" ESC_CODE_RESET, color);
        print_expr(p_expr->cast.p_expr, level + 1);
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::Unary:
        std::print("{}{}(" ESC_CODE_RESET, cstring_from(p_expr->unary.kind), color);
        print_expr(p_expr->unary.p_expr, level + 1);
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::Binary:
        std::print("{}(" ESC_CODE_RESET, color);
        print_expr(p_expr->binary.p_left, level + 1);
        std::print(" {} ", cstring_from(p_expr->binary.kind));
        print_expr(p_expr->binary.p_right, level + 1);
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::Assignment:
        std::print("{}(" ESC_CODE_RESET, color);
        print_expr(p_expr->assignment.p_left, level + 1);
        std::print(" {} ", cstring_from(p_expr->assignment.kind));
        print_expr(p_expr->assignment.p_right, level + 1);
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::Grouping:
        std::print("{}(" ESC_CODE_RESET, color);
        print_expr(p_expr->grouping.p_expr, level + 1);
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::FunctionCall:
        print_expr(p_expr->function_call.p_left, level + 1);
        std::print("{}(" ESC_CODE_RESET, color);
        for (size_t i = 0; i < p_expr->function_call.arguments.count; ++i) {
            if (i > 0) {
                std::print(", ");
            }
            print_expr(p_expr->function_call.arguments[i], level + 1);
        }
        std::print("{})" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::ArraySubscript:
        print_expr(p_expr->array_subscript.p_left, level + 1);
        std::print("{}[" ESC_CODE_RESET, color);
        print_expr(p_expr->array_subscript.p_index, level + 1);
        std::print("{}]" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::FieldAccess:
        print_expr(p_expr->field_access.p_left, level + 1);
        std::print(".{}", p_expr->field_access.field_name);
        break;
    case Expr_Kind::Variable:
        std::print("{}", p_expr->variable.name);
        break;
    case Expr_Kind::InitializerList:
        std::print("{}{{" ESC_CODE_RESET, color);
        switch (p_expr->initializer_list.kind) {
        case Initializer_List_Kind::Named:
            for (size_t i = 0; i < p_expr->initializer_list.named.fields.count; ++i) {
                if (i > 0) {
                    std::print(", ");
                }
                std::print("{}: ", p_expr->initializer_list.named.fields[i].p_name->data.str);
                print_expr(p_expr->initializer_list.named.fields[i].p_expr, level + 1);
            }
            break;
        case Initializer_List_Kind::Unnamed:
            for (size_t i = 0; i < p_expr->initializer_list.unnamed.expressions.count; ++i) {
                if (i > 0) {
                    std::print(", ");
                }
                print_expr(p_expr->initializer_list.unnamed.expressions[i], level + 1);
            }
            break;
        default:
            unreachable();
        }
        std::print("{}}}" ESC_CODE_RESET, color);
        break;
    case Expr_Kind::True: std::print("true"); break;
    case Expr_Kind::False: std::print("false"); break;
    case Expr_Kind::Null: std::print("null"); break;
    default:
        unreachable();
    }
}

void println_stmt(const Stmt &stmt, size_t level)
{
    size_t indent = 4 * level;
    std::print("{:>{}}", "", indent);

    switch (stmt.kind) {
    case Stmt_Kind::Break:
        std::print("break ");
        if (stmt._break.p_label) {
            std::print("{}", stmt._break.p_label->data.str);
        }
        std::print(";");
        break;
    case Stmt_Kind::Continue:
        std::print("continue ");
        if (stmt._continue.p_label) {
            std::print("{}", stmt._continue.p_label->data.str);
        }
        std::print(";");
        break;
    case Stmt_Kind::Return:
        std::print("return ");
        print_expr(stmt._return.p_expr);
        std::print(";");
        break;
    case Stmt_Kind::If:
        std::print("if (");
        print_expr(stmt._if.if_branch.p_condition);
        std::println(") {{");
        for (size_t i = 0; i < stmt._if.if_branch.statements.count; ++i) {
            println_stmt(stmt._if.if_branch.statements[i], level + 1);
        }
        std::print("{:>{}}", "", indent);
        std::print("}}");
        for (size_t i = 0; i < stmt._if.else_if_branches.count; ++i) {
            const If_Branch &else_if = stmt._if.else_if_branches[i];
            std::print(" else if (");
            print_expr(else_if.p_condition);
            std::println(") {{");
            for (size_t j = 0; j < else_if.statements.count; ++j) {
                println_stmt(else_if.statements[j], level + 1);
            }
            std::print("{:>{}}", "", indent);
            std::print("}}");
        }
        if (stmt._if.else_statements.count > 0) {
            std::println(" else {{");
            for (size_t i = 0; i < stmt._if.else_statements.count; ++i) {
                println_stmt(stmt._if.else_statements[i], level + 1);
            }
            std::print("{:>{}}", "", indent);
            std::print("}}");
        }
        break;
    case Stmt_Kind::Expr:
        print_expr(stmt.p_expr);
        std::print(";");
        break;
    case Stmt_Kind::Loop:
        if (stmt.loop.p_label) {
            std::print("{}: ", stmt.loop.p_label->data.str);
        }
        std::print("loop (");
        if (stmt.loop.before.is_some()) {
            print_variable_definition(stmt.loop.before.unwrap());
        }
        std::print(";");
        if (stmt.loop.p_condition) {
            print_expr(stmt.loop.p_condition);
        }
        std::print(";");
        if (stmt.loop.p_after) {
            print_expr(stmt.loop.p_after);
        }
        std::println(") {{");
        for (size_t i = 0; i < stmt.loop.body.count; ++i) {
            println_stmt(stmt.loop.body[i], level + 1);
        }
        std::print("{:>{}}", "", indent);
        std::print("}}");
        break;
    case Stmt_Kind::Scope:
        std::println("{{");
        for (size_t i = 0; i < stmt.scope_block.statements.count; ++i) {
            println_stmt(stmt.scope_block.statements[i], level + 1);
        }
        std::print("{:>{}}", "", indent);
        std::print("}}");
        break;
    case Stmt_Kind::VariableDefinition:
        print_variable_definition(stmt.variable_definition);
        std::print(";");
        break;
    default:
        unreachable();
    }
    std::print("\n");
}

// Does not print the semicolon at the end of it.
void print_variable_definition(const Variable_Definition &var_def)
{
    std::print("{} {}", var_def.is_const ? "const" : "let", var_def.name);
    if (var_def.type_annotation.is_some()) {
        std::print(": ");
        print_type_annotation(var_def.type_annotation.unwrap());
    }
    if (var_def.p_initializer) {
        std::print(" = ");
        print_expr(var_def.p_initializer);
    }
}

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
        case_infix(Equal, parse_assignment, Assignment);
        case_infix(PlusEqual, parse_assignment, Assignment);
        case_infix(MinusEqual, parse_assignment, Assignment);
        case_infix(StarEqual, parse_assignment, Assignment);
        case_infix(DivEqual, parse_assignment, Assignment);
        case_infix(ModuloEqual, parse_assignment, Assignment);
        case_infix(ShiftLeftEqual, parse_assignment, Assignment);
        case_infix(ShiftRightEqual, parse_assignment, Assignment);
        case_infix(BitwiseAndEqual, parse_assignment, Assignment);
        case_infix(BitwiseXorEqual, parse_assignment, Assignment);
        case_infix(BitwiseOrEqual, parse_assignment, Assignment);
        case_prefix(Identifier, parse_variable, None);
        case_prefix(BraceLeft, parse_initializer_list, None);
        case_prefix(Int_Literal, parse_number, None);
        case_prefix(Float_Literal, parse_number, None);
        case_prefix(Char_Literal, parse_number, None);
        case_prefix(String_Literal, parse_string_lit, None);
        case_prefix(True, parse_literal, None);
        case_prefix(False, parse_literal, None);
        case_prefix(Null, parse_literal, None);
        case_both(ParenLeft, parse_tuple_or_grouping, parse_call, Level1);
        case_infix(BracketLeft, parse_array_subscript, Level1);
        case_infix(Dot, parse_dot, Level1);
        // TODO: I am not sure if I should change the precedence from Level2 to None on these unary ops.
        case_prefix(Cast, parse_cast, Level2);
        case_prefix(Not, parse_unary, Level2);
        case_prefix(BitwiseNot, parse_unary, Level2);
        case_both(Minus, parse_unary, parse_binary, AddSub);
        case_both(Plus, parse_unary, parse_binary, AddSub);
        case_both(Star, parse_unary, parse_binary, MulDivMod);
        case_infix(Div, parse_binary, MulDivMod);
        case_infix(Modulo, parse_binary, MulDivMod);
        case_infix(Or, parse_binary, LogicalOr);
        case_infix(And, parse_binary, LogicalAnd);
        case_infix(BitwiseOr, parse_binary, BitwiseOr);
        case_infix(BitwiseXor, parse_binary, BitwiseXor);
        case_both(Ampersand, parse_unary, parse_binary, BitwiseAnd);
        case_infix(EqualEqual, parse_binary, Equality);
        case_infix(NotEqual, parse_binary, Equality);
        case_infix(LessThan, parse_binary, Comparison);
        case_infix(GreaterThan, parse_binary, Comparison);
        case_infix(LessEqual, parse_binary, Comparison);
        case_infix(GreaterEqual, parse_binary, Comparison);
        case_infix(ShiftLeft, parse_binary, Bitshift);
        case_infix(ShiftRight, parse_binary, Bitshift);
    }
    return Parse_Rule{ .prefix = nullptr, .infix = nullptr, .precedence = Precedence::None };
#undef case_none
#undef case_prefix
#undef case_infix
#undef case_both
}

void Parser::print_results()
{
    for (const auto &[name, def] : function_definitions) {
        std::println("fn {}:", name);
        for (size_t i = 0; i < def.statements.count; ++i) {
            println_stmt(def.statements[i], 1);
        }
        std::print("\n"); // for some extra space between functions.
    }
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

const Token *Parser::consume(Token_Kind expected, const char *message)
{
    const Token *consumed = p_current_token;
    if (consumed->kind != expected) {
        error_at(*p_lexer, consumed->location, "{}", message);
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
    if (!function_definitions.contains("main")) {
        error_unlocated(
            "Expected a main function as an entry point, but none was found.\n\n" ESC_CODE_RESET
            "The entry point has the following signature:\n\n"
            "fn main() {{\n"
            "    /* your entry point code */\n"
            "}}");
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

    String_View name = consume(Token_Kind::Identifier, "Expected the struct name.")->data.str;
    consume(Token_Kind::BraceLeft, "Expected the opening '{' after the struct name");
    Dynamic_Array<Typed_Identifier_Group> fields = {};

    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Typed_Identifier_Group field = parse_typed_identifier_group();
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma, "Expected a ',' after a single type annotated field or group of them");
        } else if (peek().kind == Token_Kind::Comma) {
            advance(); // trailing comma is optional.
        }
        fields.append(field);
    }
    consume(Token_Kind::BraceRight, "Expected the closing '}' of the struct");

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

    String_View name = consume(Token_Kind::Identifier, "Expected the union name")->data.str;
    consume(Token_Kind::BraceLeft, "Expected the opening '{' after the union name");
    Dynamic_Array<Typed_Identifier_Group> fields = {};

    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Typed_Identifier_Group field = parse_typed_identifier_group();
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma, "Expected a comma after a single type annotated field or a group of them");
        } else if (peek().kind == Token_Kind::Comma) {
            advance(); // trailing comma is optional.
        }
        fields.append(field);
    }
    consume(Token_Kind::BraceRight, "Expected the closing '}' of the union");

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

    String_View name = consume(Token_Kind::Identifier, "Expected the enum name")->data.str;
    consume(Token_Kind::BraceLeft, "Expected the opening '{' after the enum name");

    Dynamic_Array<Enum_Listing> listings = {};
    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        const Token *identifier = consume(Token_Kind::Identifier, "Expected an enumeration listing");
        if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Comma, "Expected a comma after an enumeration listing");
        }
        Enum_Listing listing = {
            .name = identifier->data.str,
            .location = identifier->location,
            .value = listings.count,
        };
        listings.append(listing);
    }
    consume(Token_Kind::BraceRight, "Expected the closing '}' of the enum");

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

    String_View name;
    if (is_const) {
        name = consume(Token_Kind::Identifier, "Expected the constant name")->data.str;
    } else {
        name = consume(Token_Kind::Identifier, "Expected the variable name")->data.str;
    }

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

    if (is_const) {
        consume(Token_Kind::Semicolon, "Expected a ';' to finish the constant definition");
    } else {
        consume(Token_Kind::Semicolon, "Expected a ';' to finish the variable definition");
    }

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
    case Token_Kind::Return: return parse_return_stmt();
    case Token_Kind::If: return parse_if_stmt();
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
    return Stmt{ .scope_block = {.statements = stmts}, .kind = Stmt_Kind::Scope };
}

Stmt Parser::parse_labeled_loop_stmt()
{
    debug_assert(peek().kind == Token_Kind::HashQuote);
    advance();
    const Token *p_label = consume(Token_Kind::Identifier, "Expected the name of the label");
    consume(Token_Kind::Colon, "Expected a ':' after the label");

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
        consume(Token_Kind::Semicolon, "Expected a ';' after the variable definition part of the for loop");
    }

    Expr *p_condition = nullptr;
    if (peek().kind != Token_Kind::Semicolon) {
        p_condition = parse_expr();
    }
    consume(Token_Kind::Semicolon, "Expected a ';' after the condition part of the for loop");

    Expr *p_after = nullptr;
    if (peek().kind != Token_Kind::Semicolon) {
        p_after = parse_expr();
    }
    consume(Token_Kind::Semicolon, "Expected a ';' after the post-iteration-expression part of the for loop");

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
    consume(Token_Kind::Semicolon, "Expected a ';' to end the break statement");

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
    consume(Token_Kind::Semicolon, "Expected a ';' to end the continue statement");

    return Stmt{ ._continue = { .p_label = p_label }, .location = loc, .kind = Stmt_Kind::Continue };
}

Stmt Parser::parse_return_stmt()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::Return);
    advance();

    Expr *p_expr = parse_expr();
    consume(Token_Kind::Semicolon, "Expected a ';' to end the return statement");

    return Stmt{ ._return = { .p_expr = p_expr }, .location = loc, .kind = Stmt_Kind::Return };
}

Stmt Parser::parse_if_stmt()
{
    Location loc = peek().location;
    debug_assert(peek().kind == Token_Kind::If);
    advance();

    Stmt_If _if = {};

    _if.if_branch.p_condition = parse_expr();
    _if.if_branch.statements = parse_scope_block();

    while (peek().kind == Token_Kind::Else && peek_next().kind == Token_Kind::If) {
        advance(2);
        If_Branch else_if_branch = {};
        else_if_branch.p_condition = parse_expr();
        else_if_branch.statements = parse_scope_block();
        _if.else_if_branches.append(else_if_branch);
    }

    if (peek().kind == Token_Kind::Else) {
        advance();
        _if.else_statements = parse_scope_block();
    }

    return Stmt{ ._if = _if, .location = loc, .kind = Stmt_Kind::If };
}

Stmt Parser::parse_expr_stmt()
{
    Location loc = peek().location;
    Expr *p_expr = parse_expr();
    consume(Token_Kind::Semicolon, "Expected a ';' to end the expression statement");
    return Stmt{ .p_expr = p_expr, .location = loc, .kind = Stmt_Kind::Expr };
}

Dynamic_Array<Stmt> Parser::parse_scope_block()
{
    consume(Token_Kind::BraceLeft, "Expected a '{' to start a scope block");
    Dynamic_Array<Stmt> statements = {};
    while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
        Stmt stmt = parse_stmt();
        statements.append(stmt);
    }
    consume(Token_Kind::BraceRight, "Expected a '}' to end the scope block");

    return statements;
}

Typed_Identifier_Group Parser::parse_typed_identifier_group()
{
    Dynamic_Array<const Token *> identifiers = {};
    const Token *first = consume(Token_Kind::Identifier,
        "Expected an identifier (or comma separated group of them) with a ':' and a type after it (or them)");
    identifiers.append(first);

    while (!is_eof() && peek().kind == Token_Kind::Comma) {
        advance(); // consume the comma.
        const Token *identifier = consume(Token_Kind::Identifier,
            "Expected an identifier in the typed identifier group");
        identifiers.append(identifier);
    }
    consume(Token_Kind::Colon, "Expected a colon to specify the type of the identifier(s)");
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
                consume(Token_Kind::Comma,
                    "Expected a ',' to continue (or a closing ')' to end) the tuple type annotation");
            }
            types.append(type_annotation);
        }
        consume(Token_Kind::ParenRight, "Expected a closing ')' to end the tuple type annotation");
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
            size_t array_count = peek().data.int_literal;
            advance();
            consume(Token_Kind::BracketRight, "Expected a closing ']' to end the array type annotation");
            Type_Annotation *p_type_annotation = type_annotation_pool.append();
            *p_type_annotation = parse_type_annotation();
            return Type_Annotation{
                .array = { .p_annotation = p_type_annotation, .count = array_count },
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

    String_View name = consume(Token_Kind::Identifier, "Expected the name of the function")->data.str;
    consume(Token_Kind::ParenLeft, "Expected the opening '(' of the function signature");

    Dynamic_Array<Typed_Identifier_Group> args = {};
    while (!is_eof() && peek().kind != Token_Kind::ParenRight) {
        Typed_Identifier_Group arg = parse_typed_identifier_group();
        if (!is_eof() && peek().kind != Token_Kind::ParenRight) {
            consume(Token_Kind::Comma, "Expected a ',' to continue (or a closing ')' to end) the function arguments");
        }
        args.append(arg);
    }
    consume(Token_Kind::ParenRight, "Expected the closing ')' of the function signature");

    Type_Annotation return_type;
    if (peek().kind != Token_Kind::BraceLeft) {
        consume(Token_Kind::Arrow,
            "Expected a '->' (or the opening '{' which indicates void) to indicate the return type");
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

Expr *Parser::parse_literal()
{
    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;

    switch (peek_prev().kind) {
    case Token_Kind::False: p_expr->kind = Expr_Kind::False; break;
    case Token_Kind::True: p_expr->kind = Expr_Kind::True; break;
    case Token_Kind::Null: p_expr->kind = Expr_Kind::Null; break;
    default: unreachable();
    }
    return p_expr;
}

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

Expr *Parser::parse_string_lit()
{
    debug_assert(peek_prev().kind == Token_Kind::String_Literal);

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::StringLiteral;
    p_expr->string_literal = peek_prev().data.str;

    return p_expr;
}

Expr *Parser::parse_initializer_list()
{
    debug_assert(peek_prev().kind == Token_Kind::BraceLeft);

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::InitializerList;
    p_expr->initializer_list.type_annotation = {
        .kind = Type_Annotation_Kind::None, // The type of the initializer list is unresolved during parsing.
    };

    if (peek().kind == Token_Kind::Dot) {
        // The initializer list is for a struct, it has named fields.
        p_expr->initializer_list.kind = Initializer_List_Kind::Named;

        while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            consume(Token_Kind::Dot, "Expected a '.' before the name of a struct/union field");
            const Token *p_name = consume(Token_Kind::Identifier, "Expected the name of a struct/union field");
            consume(Token_Kind::Equal, "Expected a '=' after the name of the struct/union field");

            Expr *p_field_value = parse_precedence(get_next_level(Precedence::None));
            if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
                consume(Token_Kind::Comma,
                    "Expected a ',' to continue (or a closing '}' to end) the initializer list");
            }
            p_expr->initializer_list.named.fields.append({p_name, p_field_value});
        }
    } else {
        // The initializer list if for an array, the elements are not named.
        p_expr->initializer_list.kind = Initializer_List_Kind::Unnamed;

        while (!is_eof() && peek().kind != Token_Kind::BraceRight) {
            Expr *p_element = parse_precedence(get_next_level(Precedence::None));
            if (!is_eof() && peek().kind != Token_Kind::BraceRight) {
                consume(Token_Kind::Comma,
                    "Expected a ',' to continue (or a closing '}' to end) the initializer list");
            }
            p_expr->initializer_list.unnamed.expressions.append(p_element);
        }
    }

    consume(Token_Kind::BraceRight, "Expected the closing '}' of the initializer list");

    return p_expr;
}

Expr *Parser::parse_variable()
{
    debug_assert(peek_prev().kind == Token_Kind::Identifier);

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::Variable;
    p_expr->variable = {
        .type_annotation = { .kind = Type_Annotation_Kind::None }, // At the moment of parsing, variables have unresolved types.
        .name = peek_prev().data.str,
    };

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

        if (peek().kind == Token_Kind::ParenRight) {
            // It is a tuple with a single expression: (expr,)
            advance();
            p_expr->tuple.expressions.reserve(1);
            p_expr->tuple.expressions.append(p_first_expr);
        } else {
            // It is a tuple with many expressions: (expr0, expr1, ...)
            p_expr->tuple.expressions.reserve(2); // In practice most tuples are pairs.
            p_expr->tuple.expressions.append(p_first_expr);
            while (!is_eof() && peek().kind != Token_Kind::ParenRight) {
                Expr *p_expr_inside_tuple = parse_precedence(get_next_level(Precedence::None));
                if (!is_eof() && peek().kind != Token_Kind::ParenRight) {
                    consume(Token_Kind::Comma,
                        "Expected a ',' to continue (or a closing ')' to end) the tuple expression");
                }
                p_expr->tuple.expressions.append(p_expr_inside_tuple);
            }
            consume(Token_Kind::ParenRight, "Expected the closing ')' of the tuple expression");
        }
    } else {
        // It is just a grouping.
        consume(Token_Kind::ParenRight, "Expected the closing ')' of the grouping");
        p_expr->kind = Expr_Kind::Grouping;
        p_expr->grouping.p_expr = p_first_expr;
    }

    return p_expr;
}

Expr *Parser::parse_cast()
{
    debug_assert(peek_prev().kind == Token_Kind::Cast);
    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Cast;
    p_expr->location = peek_prev().location;

    consume(Token_Kind::ParenLeft, "Expected the opening '(' of the cast, after which the type is specified");
    p_expr->cast.type_annotation = parse_type_annotation();
    consume(Token_Kind::ParenRight, "Expected the closing ')' of the cast, after which the casted expression is provided");

    p_expr->cast.p_expr = parse_precedence(Precedence::Level2);

    return p_expr;
}

Expr *Parser::parse_unary()
{
    Token op = peek_prev();
    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Unary;
    p_expr->location = peek_prev().location;

    Expr *p_operated = parse_precedence(Precedence::Level2);

#define case_unary(token_kind, unary_expr_kind)\
    case Token_Kind::token_kind:\
        p_expr->unary = { .p_expr = p_operated, .kind = Unary_Expr_Kind::unary_expr_kind };\
        break

    switch (op.kind) {
        case_unary(Plus, Plus);
        case_unary(Minus, Minus);
        case_unary(Star, Dereference);
        case_unary(Ampersand, Addressof);
        case_unary(Not, LogicalNot);
        case_unary(BitwiseNot, BitwiseNot);
    default:
        unreachable();
    }

    return p_expr;
#undef case_unary
}

Expr *Parser::parse_binary(Expr *p_left)
{
    Token op = peek_prev();
    Parse_Rule rule = get_parse_rule(op.kind);

    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Binary;
    p_expr->location = peek_prev().location;

    Expr *p_right = parse_precedence(get_next_level(rule.precedence));

#define case_binary(token_kind, binary_expr_kind)\
    case Token_Kind::token_kind:\
        p_expr->binary = { p_left, p_right, Binary_Expr_Kind::binary_expr_kind };\
        break

    switch (op.kind) {
        case_binary(Plus, Add);
        case_binary(Minus, Sub);
        case_binary(Star, Mul);
        case_binary(Div, Div);
        case_binary(Modulo, Mod);
        case_binary(Or, LogicalOr);
        case_binary(And, LogicalAnd);
        case_binary(BitwiseOr,  BitwiseOr);
        case_binary(BitwiseXor,  BitwiseXor);
        case_binary(Ampersand, BitwiseAnd);
        case_binary(EqualEqual, Equal);
        case_binary(NotEqual, NotEqual);
        case_binary(LessThan, LessThan);
        case_binary(GreaterThan, GreaterThan);
        case_binary(LessEqual, LessEqual);
        case_binary(GreaterEqual, GreaterEqual);
        case_binary(ShiftLeft, ShiftLeft);
        case_binary(ShiftRight, ShiftRight);
    default:
        unreachable();
    }

    return p_expr;
#undef case_binary
}

Expr *Parser::parse_assignment(Expr *p_left)
{
    Token op = peek_prev();
    Parse_Rule rule = get_parse_rule(op.kind);

    Expr *p_expr = expression_pool.append();
    p_expr->kind = Expr_Kind::Assignment;
    p_expr->location = op.location;

    Expr *p_right = parse_precedence(get_next_level(rule.precedence));

#define case_assignment(token_kind, assignment_kind)\
    case Token_Kind::token_kind:\
        p_expr->assignment = { p_left, p_right, Assignment_Kind::assignment_kind };\
        break

    switch (op.kind) {
        case_assignment(Equal, Equal);
        case_assignment(PlusEqual, AddEqual);
        case_assignment(MinusEqual, SubEqual);
        case_assignment(StarEqual, MulEqual);
        case_assignment(DivEqual, DivEqual);
        case_assignment(ModuloEqual, ModEqual);
        case_assignment(ShiftLeftEqual, ShiftLeftEqual);
        case_assignment(ShiftRightEqual, ShiftRightEqual);
        case_assignment(BitwiseAndEqual, BitwiseAndEqual);
        case_assignment(BitwiseXorEqual, BitwiseXorEqual);
        case_assignment(BitwiseOrEqual, BitwiseOrEqual);
    default:
        unreachable();
    }

    return p_expr;
#undef case_assignment
}

Expr *Parser::parse_call(Expr *p_left)
{
    debug_assert(peek_prev().kind == Token_Kind::ParenLeft);

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::FunctionCall;
    p_expr->function_call.p_left = p_left;

    while (peek().kind != Token_Kind::ParenRight) {
        Expr *p_arg = parse_precedence(get_next_level(Precedence::None));
        if (peek().kind != Token_Kind::ParenRight) {
            consume(Token_Kind::Comma,
                "Expected a ',' to continue (or a closing ')' to end) the function call expression");
        }
        p_expr->function_call.arguments.append(p_arg);
    }
    consume(Token_Kind::ParenRight, "Expected the closing ')' of the function call expression");

    return p_expr;
}

Expr *Parser::parse_array_subscript(Expr *p_left)
{
    debug_assert(peek_prev().kind == Token_Kind::BracketLeft);

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::ArraySubscript;
    p_expr->array_subscript.p_left = p_left;
    p_expr->array_subscript.p_index = parse_precedence(get_next_level(Precedence::None));

    consume(Token_Kind::BracketRight, "Expected the closing ']' of the array subscript expression");

    return p_expr;
}

Expr *Parser::parse_dot(Expr *p_left)
{
    debug_assert(peek_prev().kind == Token_Kind::Dot);
    String_View name = consume(Token_Kind::Identifier, "Expected struct/union field name after '.'")->data.str;

    Expr *p_expr = expression_pool.append();
    p_expr->location = peek_prev().location;
    p_expr->kind = Expr_Kind::FieldAccess;
    p_expr->field_access = { .p_left = p_left, .field_name = name };

    return p_expr;
}


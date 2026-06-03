#include "type_checker.h"
#include "core/dynamic_stack.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>

void Type_Checker::check()
{
    check_user_defined_type_completeness();
    resolve_global_variable_types();
}

// Gets all of the user defined types that are required to be complete
// so that it is possible to calculate the size of a struct or union.
static void get_nested_sized_user_defined_types(
    const Type_Annotation *p_type_annotation,
    Dynamic_Array<const Type_Annotation*> *nested)
{
    switch (p_type_annotation->kind) {
    case Type_Annotation_Kind::Array:
        get_nested_sized_user_defined_types(
            p_type_annotation->array.p_annotation, nested);
        break;
    case Type_Annotation_Kind::Pointer:
        // Stop walking the tree. All pointers have a known size.
        break;
    case Type_Annotation_Kind::Slice:
        // Stop walking the tree. All slices are a pointer and a length.
        break;
    case Type_Annotation_Kind::Tuple:
        for (size_t i = 0; i < p_type_annotation->tuple.types.count; ++i) {
            const Type_Annotation *type_annotation_i = p_type_annotation->tuple.types[i];
            get_nested_sized_user_defined_types(type_annotation_i, nested);
        }
        break;
    case Type_Annotation_Kind::UserDefined:
        nested->append(p_type_annotation);
        break;
    }
}

struct Sized_Type_Node {
    String_View name;
    Location location;

    bool operator==(const Sized_Type_Node &other) const
    {
        // Only the name is relevant for comparison with other nodes.
        return this->name.equals(other.name);
    }
};

template<>
struct std::hash<Sized_Type_Node> {
    size_t operator()(const Sized_Type_Node &node) const noexcept
    {
        // Only the name is relevant for hashing.
        return std::hash<String_View>{}(node.name);
    }
};

// Here I use std::vector instead of my replacement because in this case I want it to use RAII.
// It is an adjacency list.
using Sized_Type_Graph = std::unordered_map<String_View, std::vector<Sized_Type_Node>>;

// This function makes sure that all structs/unions are "complete types".
// See https://learn.microsoft.com/en-us/cpp/c-language/incomplete-types
void Type_Checker::check_user_defined_type_completeness()
{
    Sized_Type_Graph graph = {};

    // First pass just adds the nodes.
    for (const auto &[name, _] : p_parser->struct_definitions) {
        debug_assert(!graph.contains(name));
        graph[name] = {};
    }
    for (const auto &[name, _] : p_parser->union_definitions) {
        debug_assert(!graph.contains(name));
        graph[name] = {};
    }

    // Second pass adds the edges.
    {
        Dynamic_Array<const Type_Annotation*> nested = {};
        for (const auto &[struct_name, struct_def] : p_parser->struct_definitions) {
            for (const Typed_Identifier_Group &field : struct_def.fields) {
                nested.count = 0; // Reusing the same dynamic array so allocations happen less often.
                get_nested_sized_user_defined_types(field.p_type_annotation, &nested);
                for (const Type_Annotation *annotation : nested) {
                    debug_assert(annotation->kind == Type_Annotation_Kind::UserDefined);
                    graph[struct_name].push_back({
                        .name = annotation->user_defined.name,
                        .location = annotation->location,
                    });
                }
            }
        }
        for (const auto &[union_name, union_def] : p_parser->union_definitions) {
            for (const Typed_Identifier_Group &field : union_def.fields) {
                nested.count = 0; // Reusing the same dynamic array so allocations happen less often.
                get_nested_sized_user_defined_types(field.p_type_annotation, &nested);
                for (const Type_Annotation *annotation : nested) {
                    debug_assert(annotation->kind == Type_Annotation_Kind::UserDefined);
                    graph[union_name].push_back({
                        .name = annotation->user_defined.name,
                        .location = annotation->location,
                    });
                }
            }
        }
        nested.destroy();
    }

    // Third pass checks cycles in the graph.
    auto dfs_check_cycle = [this](const Sized_Type_Graph& graph, Sized_Type_Node start_node) -> void {
        std::unordered_set<Sized_Type_Node> visited = {};
        Stack<Sized_Type_Node>::Scoped to_visit = {};
        to_visit.push(start_node);
        while (!to_visit.is_empty()) {
            Sized_Type_Node node = to_visit.pop();
            if (visited.contains(node)) {
                error_at(*this->p_parser->p_lexer, node.location,
                    "{} is an incomplete type. Consider adding type indirection, for example: *{}",
                    node.name, node.name);
            } else {
                visited.insert(node);
                if (!graph.contains(node.name)) {
                    error_at(*this->p_parser->p_lexer, node.location,
                        "{} is an undefined type in the language and in the program.", node.name);
                }
                for (const Sized_Type_Node &neighbor : graph.at(node.name)) {
                    to_visit.push(neighbor);
                }
            }
        }
    };
    for (const auto &[_, struct_def] : p_parser->struct_definitions) {
        dfs_check_cycle(graph, { .name = struct_def.name, .location = struct_def.location });
    }
    for (const auto &[_, union_def] : p_parser->union_definitions) {
        dfs_check_cycle(graph, { .name = union_def.name, .location = union_def.location });
    }
}

// Navigates a type annotation tree, looking for unresolved user defined types,
// then finding which type it is (struct or union).
static void resolve_type_tree(Parser *p_parser, Type_Annotation *p_annotation)
{
    debug_assert(p_annotation);
    switch (p_annotation->kind) {
    case Type_Annotation_Kind::Array:
        resolve_type_tree(p_parser, p_annotation->array.p_annotation);
        break;
    case Type_Annotation_Kind::Pointer:
        resolve_type_tree(p_parser, p_annotation->pointer.p_annotation);
        break;
    case Type_Annotation_Kind::Slice:
        resolve_type_tree(p_parser, p_annotation->slice.p_annotation);
        break;
    case Type_Annotation_Kind::Tuple:
        for (Type_Annotation *p_subtype : p_annotation->tuple.types) {
            resolve_type_tree(p_parser, p_subtype);
        }
        break;
    case Type_Annotation_Kind::UserDefined:
        if (p_annotation->user_defined.kind == User_Defined_Kind::unresolved_type) {
            String_View type_name = p_annotation->user_defined.name;
            if (p_parser->struct_definitions.contains(type_name)) {
                p_annotation->user_defined.kind = User_Defined_Kind::Struct;
            } else if (p_parser->union_definitions.contains(type_name)) {
                p_annotation->user_defined.kind = User_Defined_Kind::Union;
            } else if (p_parser->enum_definitions.contains(type_name)) {
                p_annotation->user_defined.kind = User_Defined_Kind::Enum;
            } else {
                error_at(*p_parser->p_lexer, p_annotation->location,
                    "Expected an unresolved struct, union, or enumeration.");
            }
        }
        break;
    default:
        unreachable();
    }
}

static bool are_types_equal(Type_Annotation *a, Type_Annotation *b)
{
    debug_assert(a && b);
    using enum Type_Annotation_Kind;
    switch (a->kind) {
    case Array: {
        if (b->kind != Array) {
            return false;
        }
        return are_types_equal(a->array.p_annotation, b->array.p_annotation);
    }
    case Builtin: {
        if (b->kind != Builtin) {
            return false;
        }
        return a->builtin.keyword == b->builtin.keyword;
    }
    case Pointer: {
        if (b->kind != Pointer) {
            return false;
        }
        return are_types_equal(a->pointer.p_annotation, b->pointer.p_annotation);
    }
    case Slice: {
        if (b->kind != Slice) {
            return false;
        }
        return are_types_equal(a->slice.p_annotation, b->slice.p_annotation);
    }
    case Tuple: {
        if (b->kind != Tuple) {
            return false;
        }
        if (a->tuple.types.count != b->tuple.types.count) {
            return false;
        }
        bool is_same = false;
        for (size_t i = 0; i < a->tuple.types.count; ++i) {
            is_same = is_same && are_types_equal(a->tuple.types[i], b->tuple.types[i]);
        }
        return is_same;
    }
    case UserDefined: {
        if (b->kind != UserDefined) {
            return false;
        }
        if (a->user_defined.kind != b->user_defined.kind) {
            return false;
        }
        return a->user_defined.name.equals(b->user_defined.name);
    }
    default:
        unreachable();
    }
}

#define precision_loss_error(lexer, location, p_type_1, p_type_2)\
    error_print_prefix();\
    eprint("The conversion from type ");\
    print_type_annotation(p_type_1);\
    eprint(" to type ");\
    print_type_annotation(p_type_2);\
    eprintln(" loses precision. Use an explicit cast." ESC_CODE_RESET);\
    (lexer).print_error_message_line(location);\
    eprintln("");\
    my_exit(1)

#define incompatible_types_error(lexer, location, p_type_1, p_type_2)\
    error_print_prefix();\
    eprint("The type ");\
    print_type_annotation(p_type_1);\
    eprint(" is incompatible with the type ");\
    print_type_annotation(p_type_2);\
    eprintln("." ESC_CODE_RESET);\
    (lexer).print_error_message_line(location);\
    eprintln("");\
    my_exit(1)

#define type_error(lexer, location, p_type_annotation, format_string, ...)\
    error_print_prefix();\
    eprint("The type ");\
    print_type_annotation(p_type_annotation);\
    eprintln(" " format_string ESC_CODE_RESET, ##__VA_ARGS__);\
    (lexer).print_error_message_line(location);\
    eprintln("");\
    my_exit(1)

static Type_Annotation *get_upcast(Lexer *p_lexer, Location loc, Type_Annotation *a, Type_Annotation *b)
{
    if (are_types_equal(a, b)) {
        return a;
    }
    if (a->is_integer() && b->is_integer()) {
        const size_t sizeof_a = get_builtin_type_size_in_bytes(a->builtin.keyword);
        const size_t sizeof_b = get_builtin_type_size_in_bytes(b->builtin.keyword);
        // When they both have the same size, but a different signedness, an explicit cast is required.
        if ((sizeof_a == sizeof_b) && (a->builtin.keyword != b->builtin.keyword)) {
            precision_loss_error(*p_lexer, loc, a, b);
        }
        return (sizeof_a > sizeof_b) ? a : b;
    }
    if (a->is_float() && b->is_float()) {
        return ((int)a->builtin.keyword > (int)b->builtin.keyword) ? a : b;
    }
    if (a->is_integer() && b->is_float()) {
        return b;
    }
    if (a->is_float() && b->is_integer()) {
        return a;
    }
    if (a->is_void_pointer() && b->is_pointer()) {
        return b;
    }
    if (a->is_pointer() && b->is_void_pointer()) {
        return a;
    }
    return nullptr;
}

static bool force_cast(Type_Annotation *to, Type_Annotation *from)
{
    if (from->is_annonymous_object()) {
        switch (to->kind) {
        case Type_Annotation_Kind::Array:
            TODO("Resolving annonymous arrays.");
            break;
        case Type_Annotation_Kind::UserDefined:
            TODO("Resolving annonymous structs.");
            break;
        default:
            return false;
        }
    }
    if (are_types_equal(to, from)) {
        return true;
    }
    if (to->is_number() && from->is_number()) {
        return true;
    }
    if (to->is_pointer() && from->is_pointer()) {
        return true;
    }
    return false;
}

using Symbol_Table = std::unordered_map<String_View, Type_Annotation*>;

// Recursively navigates the expression tree with DFS in post-order traversal
// and resolves the type of each expression, making sure that n-ary expressions
// have compatible types.
static void resolve_expr_tree(Parser *p_parser, Symbol_Table &symbol_table, Expr *p_expr)
{
    // At first the type should be unresolved.
    // Type conversions are an edge case, because they already know their type at parse time
    // but they still have to check that the operand is compatible with that type.
    if (p_expr->kind != Expr_Kind::Cast) {
        debug_assert(p_expr->p_type_annotation == nullptr);
    }

    Lexer *p_lexer = p_parser->p_lexer;

    switch (p_expr->kind) {

    case Expr_Kind::Number: {
        // TODO: Support all kinds of integer literals.
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
        switch (p_expr->number.kind) {
        case Number_Kind::Integer:
            p_expr->p_type_annotation->builtin.keyword = Token_Kind::Int;
            break;
        case Number_Kind::Char:
            p_expr->p_type_annotation->builtin.keyword = Token_Kind::Char;
            break;
        case Number_Kind::Float:
            p_expr->p_type_annotation->builtin.keyword = Token_Kind::F64;
            break;
        default:
            unreachable();
        }
    } break;

    case Expr_Kind::StringLiteral: {
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
        p_expr->p_type_annotation->builtin.keyword = Token_Kind::String;
    } break;

    case Expr_Kind::Tuple: {
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->kind = Type_Annotation_Kind::Tuple;
        for (Expr *p_subexpr : p_expr->tuple.expressions) {
            resolve_expr_tree(p_parser, symbol_table, p_subexpr);
            p_expr->p_type_annotation->tuple.types.append(p_subexpr->p_type_annotation);
        }
    } break;

    case Expr_Kind::Cast: {
        Expr *p_casted = p_expr->cast.p_expr;
        resolve_expr_tree(p_parser, symbol_table, p_casted);
        if (!force_cast(p_expr->p_type_annotation, p_casted->p_type_annotation)) {
            incompatible_types_error(*p_lexer, p_expr->location,
                p_expr->p_type_annotation, p_casted->p_type_annotation);
        }
    } break;

    case Expr_Kind::Unary: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->unary.p_expr);
        Expr *p_operated = p_expr->unary.p_expr;
        switch (p_expr->unary.kind) {
        case Unary_Expr_Kind::Plus:
        case Unary_Expr_Kind::Minus: {
            if (!p_operated->p_type_annotation->allows_math_operators()) {
                type_error(*p_lexer, p_expr->location, p_operated->p_type_annotation, "does not allow math operators.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::Dereference: {
            if (p_operated->p_type_annotation->kind != Type_Annotation_Kind::Pointer) {
                type_error(*p_lexer, p_expr->location, p_operated->p_type_annotation, "does not allow dereference, it must be a pointer.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation->pointer.p_annotation;
        } break;
        case Unary_Expr_Kind::Addressof: {
            if (!p_operated->is_lvalue()) {
                type_error(*p_lexer, p_expr->location, p_operated->p_type_annotation, "is not an lvalue, thus it is not addressable.");
            }
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
            p_expr->p_type_annotation->pointer.p_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::LogicalNot: {
            if (!p_operated->p_type_annotation->is_boolean()) {
                type_error(*p_lexer, p_expr->location, p_operated->p_type_annotation, "is not a boolean, logical not is invalid here.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::BitwiseNot: {
            if (!p_operated->p_type_annotation->is_integer()) {
                type_error(*p_lexer, p_expr->location, p_operated->p_type_annotation, "is not an integer type, bitwise not is invalid here.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation;
        } break;
        default:
            unreachable();
        }
    } break;

    case Expr_Kind::Binary: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->binary.p_left);
        resolve_expr_tree(p_parser, symbol_table, p_expr->binary.p_right);
        Type_Annotation *p_type_left = p_expr->binary.p_left->p_type_annotation;
        Type_Annotation *p_type_right = p_expr->binary.p_right->p_type_annotation;
        Type_Annotation *p_type = get_upcast(p_lexer, p_expr->location, p_type_left, p_type_right);
        if (p_type == nullptr) {
            incompatible_types_error(*p_lexer, p_expr->location, p_type_left, p_type_right);
        }
        switch (p_expr->binary.kind) {
        case Binary_Expr_Kind::Add:
        case Binary_Expr_Kind::Sub:
        case Binary_Expr_Kind::Mul:
        case Binary_Expr_Kind::Div:
        case Binary_Expr_Kind::Mod:
            if (!p_type->allows_math_operators()) {
                type_error(*p_lexer, p_expr->location, p_type, "does not allow math operators.");
            }
            p_expr->p_type_annotation = p_type;
            break;
        case Binary_Expr_Kind::LogicalOr:
        case Binary_Expr_Kind::LogicalAnd:
            if (!p_type->is_boolean()) {
                type_error(*p_lexer, p_expr->location, p_type, "is not a boolean.");
            }
            p_expr->p_type_annotation = p_type;
            break;
        case Binary_Expr_Kind::BitwiseOr:
        case Binary_Expr_Kind::BitwiseAnd:
        case Binary_Expr_Kind::BitwiseXor:
        case Binary_Expr_Kind::ShiftLeft:
        case Binary_Expr_Kind::ShiftRight:
            if (!p_type->is_integer()) {
                type_error(*p_lexer, p_expr->location, p_type, "is not an integer type.");
            }
            p_expr->p_type_annotation = p_type;
            break;
        case Binary_Expr_Kind::Equal:
        case Binary_Expr_Kind::NotEqual:
        case Binary_Expr_Kind::GreaterThan:
        case Binary_Expr_Kind::LessThan:
        case Binary_Expr_Kind::GreaterEqual:
        case Binary_Expr_Kind::LessEqual:
            if (p_type->kind != Type_Annotation_Kind::Builtin &&
                p_type->kind != Type_Annotation_Kind::Pointer) {
                type_error(*p_lexer, p_expr->location, p_type,
                    "is not a built-in type or pointer type (only these types can be compared).");
            }
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
            p_expr->p_type_annotation->builtin.keyword = Token_Kind::Bool;
            break;
        default:
            unreachable();
        }
    } break;

    case Expr_Kind::Assignment: {
        Expr *p_left = p_expr->assignment.p_left;
        Expr *p_right = p_expr->assignment.p_right;
        resolve_expr_tree(p_parser, symbol_table, p_right);
        resolve_expr_tree(p_parser, symbol_table, p_left);
        if (!p_expr->assignment.p_left->is_lvalue()) {
            error_at(*p_lexer, p_expr->assignment.p_left->location,
                "The left hand side of the assignment is not an lvalue.");
        }
        if (!force_cast(p_left->p_type_annotation, p_right->p_type_annotation)) {
            incompatible_types_error(*p_lexer, p_expr->location,
                p_left->p_type_annotation, p_right->p_type_annotation);
        }
    } break;

    case Expr_Kind::Grouping: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->grouping.p_expr);
        p_expr->p_type_annotation = p_expr->grouping.p_expr->p_type_annotation;
    } break;

    case Expr_Kind::FunctionCall: {
        Expr *p_function = p_expr->function_call.p_left;
        resolve_expr_tree(p_parser, symbol_table, p_function);
        if (p_function->p_type_annotation->is_function()) {
            type_error(*p_lexer, p_expr->location, p_function->p_type_annotation, "is not a function.");
        }
        for (Expr *p_argument : p_expr->function_call.arguments) {
            resolve_expr_tree(p_parser, symbol_table, p_argument);
        }
        String_View function_name = p_function->p_type_annotation->user_defined.name;
        debug_assert(p_parser->function_definitions.contains(function_name));
        Function_Definition &fn_def = p_parser->function_definitions[function_name];
        p_expr->p_type_annotation = fn_def.signature.p_return_type;
    } break;

    case Expr_Kind::ArraySubscript: {
        Expr *p_array = p_expr->array_subscript.p_left;
        resolve_expr_tree(p_parser, symbol_table, p_array);
        switch (p_array->p_type_annotation->kind) {
        case Type_Annotation_Kind::Builtin:
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            switch (p_array->p_type_annotation->builtin.keyword) {
            case Token_Kind::String:
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::U8;
                break;
            case Token_Kind::Vec2:
            case Token_Kind::Vec3:
            case Token_Kind::Vec4:
            case Token_Kind::Mat4:
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            default:
                type_error(*p_lexer, p_expr->location, p_array->p_type_annotation, "is not indexable.");
            }
            break;
        case Type_Annotation_Kind::Array:
            p_expr->p_type_annotation = p_array->p_type_annotation->array.p_annotation;
            break;
        case Type_Annotation_Kind::Pointer:
            p_expr->p_type_annotation = p_array->p_type_annotation->pointer.p_annotation;
            break;
        case Type_Annotation_Kind::Slice:
            p_expr->p_type_annotation = p_array->p_type_annotation->slice.p_annotation;
            break;
        default:
            type_error(*p_lexer, p_expr->location, p_array->p_type_annotation, "is not indexable.");
        }
        Expr *p_index = p_expr->array_subscript.p_index;
        resolve_expr_tree(p_parser, symbol_table, p_index);
        if (!p_index->p_type_annotation->is_integer()) {
            type_error(*p_lexer, p_expr->location, p_index->p_type_annotation, "is not an integer type.");
        }
    } break;

    case Expr_Kind::FieldAccess: {
        String_View field_name = p_expr->field_access.field_name;
        Expr *p_object = p_expr->field_access.p_left;
        resolve_expr_tree(p_parser, symbol_table, p_object);
        switch (p_object->p_type_annotation->kind) {
        case Type_Annotation_Kind::Builtin: {
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            switch (p_object->p_type_annotation->builtin.keyword) {
            case Token_Kind::String:
                if (field_name == String_View_literal("data")) {
                    p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
                    p_expr->p_type_annotation->pointer.p_annotation = p_parser->type_annotation_pool.append();
                    p_expr->p_type_annotation->pointer.p_annotation->kind = Type_Annotation_Kind::Builtin;
                    p_expr->p_type_annotation->pointer.p_annotation->builtin.keyword = Token_Kind::U8;
                } else if (field_name == String_View_literal("length")) {
                    p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                    p_expr->p_type_annotation->builtin.keyword = Token_Kind::Int;
                } else {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have field \"{}\", it has fields \"data\", \"length\".", field_name);
                }
                break;
            case Token_Kind::Vec2:
                if (field_name != String_View_literal("x") && 
                    field_name != String_View_literal("y")) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have field \"{}\", it has fields \"x\", \"y\".", field_name);
                }
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            case Token_Kind::Vec3:
                if (field_name != String_View_literal("x") && 
                    field_name != String_View_literal("y") &&
                    field_name != String_View_literal("z")) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have field \"{}\", it has fields \"x\", \"y\", \"z\".", field_name);
                }
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            case Token_Kind::Vec4:
                if (field_name != String_View_literal("x") && 
                    field_name != String_View_literal("y") &&
                    field_name != String_View_literal("z") &&
                    field_name != String_View_literal("w")) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have field \"{}\", it has fields \"x\", \"y\", \"z\", \"w\".", field_name);
                }
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            default:
                type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                    "does not have fields to access.");
            }
        } break;
        case Type_Annotation_Kind::Slice: {
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            if (field_name == String_View_literal("data")) {
                Type_Annotation *slice_subtype = p_object->p_type_annotation->slice.p_annotation;
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
                p_expr->p_type_annotation->pointer.p_annotation = slice_subtype;
            } else if (field_name == String_View_literal("length")) {
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::Int;
            } else {
                type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                    "does not have field \"{}\", it has fields \"data\", \"length\".", field_name);
            }
        } break;
        case Type_Annotation_Kind::Tuple: {
            size_t field_as_number = p_lexer->parse_u64(field_name, p_expr->location, 10);
            size_t element_count = p_object->p_type_annotation->tuple.types.count;
            if (field_as_number >= element_count) {
                error_at(*p_lexer, p_expr->location,
                    "The tuple field {} is out of bounds for a tuple of {} elements.",
                    field_as_number, element_count);
            }
            p_expr->p_type_annotation = p_object->p_type_annotation->tuple.types[field_as_number];
        } break;
        case Type_Annotation_Kind::UserDefined: {
            String_View type_name = p_object->p_type_annotation->user_defined.name;
            if (type_name.length == 0) {
                error_at(*p_lexer, p_expr->location, "Accessing annonymous structs is invalid.");
            }
            switch (p_object->p_type_annotation->user_defined.kind) {
            case User_Defined_Kind::Struct: {
                debug_assert(p_parser->struct_definitions.contains(type_name));
                const Struct_Definition &def = p_parser->struct_definitions[type_name];
                Type_Annotation *p_field_type = def.get_typeof_field(field_name);
                if (!p_field_type) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have a field called \"{}\".", field_name);
                }
                p_expr->p_type_annotation = p_field_type;
            } break;
            case User_Defined_Kind::Union: {
                debug_assert(p_parser->union_definitions.contains(type_name));
                const Union_Definition &def = p_parser->union_definitions[type_name];
                Type_Annotation *p_field_type = def.get_typeof_field(field_name);
                if (!p_field_type) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have a field called \"{}\".", field_name);
                }
                p_expr->p_type_annotation = p_field_type;
            } break;
            case User_Defined_Kind::Enum: {
                debug_assert(p_parser->enum_definitions.contains(type_name));
                const Enum_Definition &def = p_parser->enum_definitions[type_name];
                if (!def.contains(field_name)) {
                    type_error(*p_lexer, p_expr->location, p_object->p_type_annotation,
                        "does not have a field called \"{}\".", field_name);
                }
                p_expr->p_type_annotation = def.p_underlying_type;
            } break;
            default:
                error_at(*p_lexer, p_expr->location, "Expected a struct, union, or enumeration.");
            }
        } break;
        default:
            type_error(*p_lexer, p_expr->location, p_object->p_type_annotation, "does not have fields to access.");
        }
    } break;

    case Expr_Kind::Variable: {
        String_View name = p_expr->variable.name;
        if (p_parser->function_definitions.contains(name)) {
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::UserDefined;
            p_expr->p_type_annotation->user_defined.kind = User_Defined_Kind::Function;
            p_expr->p_type_annotation->user_defined.name = name;
        } else if (p_parser->enum_definitions.contains(name)) {
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::UserDefined;
            p_expr->p_type_annotation->user_defined.kind = User_Defined_Kind::Enum;
            p_expr->p_type_annotation->user_defined.name = name;
        } else {
            if (!symbol_table.contains(name)) {
                error_at(*p_lexer, p_expr->location, "Undefined variable or function {}.", name);
            }
            p_expr->p_type_annotation = symbol_table.at(name);
        }
    } break;

    case Expr_Kind::InitializerList: {
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        switch (p_expr->initializer_list.kind) {
        case Initializer_List_Kind::Named: {
            for (const Initializer_List::Named_Field &field : p_expr->initializer_list.named.fields) {
                resolve_expr_tree(p_parser, symbol_table, field.p_expr);
            }
            // The name of this type is unresolved yet, it is just some anonymous struct or union.
            // Later when this expression is saved in a variable or passed to a function,
            // the initializer list has to match one of the defined structs/unions in the program.
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::UserDefined;
            p_expr->p_type_annotation->user_defined.kind = User_Defined_Kind::unresolved_type;
        } break;
        case Initializer_List_Kind::Unnamed: {
            // Checking that all elements have the same type is done later when this is used.
            for (Expr *p_array_element : p_expr->initializer_list.unnamed.expressions) {
                resolve_expr_tree(p_parser, symbol_table, p_array_element);
            }
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Array;
            p_expr->p_type_annotation->array.p_annotation = nullptr;
        } break;
        default:
            unreachable();
        }
    } break;

    case Expr_Kind::True:
    case Expr_Kind::False: {
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
        p_expr->p_type_annotation->builtin.keyword = Token_Kind::Bool;
    } break;

    case Expr_Kind::Null: {
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
        p_expr->p_type_annotation->pointer.p_annotation = p_parser->type_annotation_pool.append();
        p_expr->p_type_annotation->pointer.p_annotation->kind = Type_Annotation_Kind::Builtin;
        p_expr->p_type_annotation->pointer.p_annotation->builtin.keyword = Token_Kind::Void;
    } break;

    default:
        unreachable();
    }

    // Now it should be resolved.
    debug_assert(p_expr->p_type_annotation);
}

void Type_Checker::resolve_global_variable_types()
{
    Symbol_Table symbol_table = {};

    for (String_View name : p_parser->global_variable_definitions.order) {
        const Variable_Definition &def = p_parser->global_variable_definitions.map[name];
        assert(def.p_initializer);
        resolve_expr_tree(p_parser, symbol_table, def.p_initializer);
        if (def.p_type_annotation) {
            resolve_type_tree(p_parser, def.p_type_annotation);
            if (!force_cast(def.p_type_annotation, def.p_initializer->p_type_annotation)) {
                incompatible_types_error(*p_parser->p_lexer, def.p_initializer->location,
                    def.p_type_annotation, def.p_initializer->p_type_annotation);
            }
            symbol_table[name] = def.p_type_annotation;
        } else {
            if (def.p_initializer->p_type_annotation->is_annonymous_object()) {
                error_at(*p_parser->p_lexer, def.p_initializer->location,
                    "When using a struct/array literal, you must provide the"
                    "type annotation in the variable definition.");
            }
            symbol_table[name] = def.p_initializer->p_type_annotation;
        }
    }
}

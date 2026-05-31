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
    const Type_Annotation &type_annotation,
    Dynamic_Array<Type_Annotation> *nested)
{
    switch (type_annotation.kind) {
    case Type_Annotation_Kind::Array:
        get_nested_sized_user_defined_types(
            *type_annotation.array.p_annotation, nested);
        break;
    case Type_Annotation_Kind::Pointer:
        // Stop walking the tree. All pointers have a known size.
        break;
    case Type_Annotation_Kind::Slice:
        // Stop walking the tree. All slices are a pointer and a length.
        break;
    case Type_Annotation_Kind::Tuple:
        for (size_t i = 0; i < type_annotation.tuple.types.count; ++i) {
            const Type_Annotation &type_annotation_i = type_annotation.tuple.types[i];
            get_nested_sized_user_defined_types(type_annotation_i, nested);
        }
        break;
    case Type_Annotation_Kind::UserDefined:
        nested->append(type_annotation);
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
        Dynamic_Array<Type_Annotation> nested = {};
        for (const auto &[struct_name, struct_def] : p_parser->struct_definitions) {
            for (const Typed_Identifier_Group &field : struct_def.fields) {
                nested.count = 0; // Reusing the same dynamic array so allocations happen less often.
                get_nested_sized_user_defined_types(field.type_annotation, &nested);
                for (const Type_Annotation &annotation : nested) {
                    debug_assert(annotation.kind == Type_Annotation_Kind::UserDefined);
                    graph[struct_name].push_back({
                        .name = annotation.user_defined.name,
                        .location = annotation.location,
                    });
                }
            }
        }
        for (const auto &[union_name, union_def] : p_parser->union_definitions) {
            for (const Typed_Identifier_Group &field : union_def.fields) {
                nested.count = 0; // Reusing the same dynamic array so allocations happen less often.
                get_nested_sized_user_defined_types(field.type_annotation, &nested);
                for (const Type_Annotation &annotation : nested) {
                    debug_assert(annotation.kind == Type_Annotation_Kind::UserDefined);
                    graph[union_name].push_back({
                        .name = annotation.user_defined.name,
                        .location = annotation.location,
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

#define type_error(p_expr, format_string, ...)\
    error_print_prefix();\
    eprint("The type ");\
    print_type_annotation(*(p_expr)->p_type_annotation);\
    eprintln(" " format_string ESC_CODE_RESET, ##__VA_ARGS__);\
    p_lexer->print_error_message_line((p_expr)->location);\
    eprintln("");\
    my_exit(1)

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
            p_expr->p_type_annotation->builtin.keyword = Token_Kind::U32;
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
            p_expr->p_type_annotation->tuple.types.append(*p_subexpr->p_type_annotation);
        }
    } break;

    case Expr_Kind::Cast: {
        Expr *p_casted = p_expr->cast.p_expr;
        resolve_expr_tree(p_parser, symbol_table, p_casted);
        feature_todo(*p_lexer, p_expr->location, "type-resolving casting expressions");
    } break;

    case Expr_Kind::Unary: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->unary.p_expr);
        Expr *p_operated = p_expr->unary.p_expr;
        switch (p_expr->unary.kind) {
        case Unary_Expr_Kind::Plus:
        case Unary_Expr_Kind::Minus: {
            if (!p_operated->p_type_annotation->allows_math_operators()) {
                type_error(p_operated, "does not allow math operators.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::Dereference: {
            if (p_operated->p_type_annotation->kind != Type_Annotation_Kind::Pointer) {
                type_error(p_operated, "does not allow dereference, it must be a pointer.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation->pointer.p_annotation;
        } break;
        case Unary_Expr_Kind::Addressof: {
            if (!p_operated->is_lvalue()) {
                type_error(p_operated, "is not an lvalue, thus it is not addressable.");
            }
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
            p_expr->p_type_annotation->pointer.p_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::LogicalNot: {
            if (!p_operated->p_type_annotation->is_boolean()) {
                type_error(p_operated, "is not a boolean, logical not is invalid here.");
            }
            p_expr->p_type_annotation = p_operated->p_type_annotation;
        } break;
        case Unary_Expr_Kind::BitwiseNot: {
            if (!p_operated->p_type_annotation->is_integer()) {
                type_error(p_operated, "is not an integer type, bitwise not is invalid here.");
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
        feature_todo(*p_lexer, p_expr->location, "type-resolving binary expressions");
    } break;

    case Expr_Kind::Assignment: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->assignment.p_right);
        resolve_expr_tree(p_parser, symbol_table, p_expr->assignment.p_left);
        if (!p_expr->assignment.p_left->is_lvalue()) {
            error_at(*p_lexer, p_expr->assignment.p_left->location,
                "The left hand side of the assignment is not an lvalue.");
        }
        feature_todo(*p_lexer, p_expr->location, "type-resolving assignment expressions");
    } break;

    case Expr_Kind::Grouping: {
        resolve_expr_tree(p_parser, symbol_table, p_expr->grouping.p_expr);
        p_expr->p_type_annotation = p_expr->grouping.p_expr->p_type_annotation;
    } break;

    case Expr_Kind::FunctionCall: {
        Expr *p_function = p_expr->function_call.p_left;
        resolve_expr_tree(p_parser, symbol_table, p_function);
        if (p_function->p_type_annotation->kind != Type_Annotation_Kind::Function) {
            type_error(p_function, "is not a function.");
        }
        for (Expr *p_argument : p_expr->function_call.arguments) {
            resolve_expr_tree(p_parser, symbol_table, p_argument);
        }
        String_View function_name = p_function->p_type_annotation->function.name;
        debug_assert(p_parser->function_definitions.contains(function_name));
        Function_Definition &fn_def = p_parser->function_definitions[function_name];
        p_expr->p_type_annotation = &fn_def.signature.return_type;
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
                type_error(p_array, "is not indexable.");
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
            type_error(p_array, "is not indexable.");
        }
        Expr *p_index = p_expr->array_subscript.p_index;
        resolve_expr_tree(p_parser, symbol_table, p_index);
        if (!p_index->p_type_annotation->is_integer()) {
            type_error(p_index, "is not an integer type.");
        }
    } break;

    case Expr_Kind::FieldAccess: {
        String_View field_name = p_expr->field_access.field_name;
        Expr *p_object = p_expr->field_access.p_left;
        resolve_expr_tree(p_parser, symbol_table, p_object);
        p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
        switch (p_object->p_type_annotation->kind) {
        case Type_Annotation_Kind::Builtin: {
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
                    type_error(p_object,
                        "does not have field \"{}\", it has fields \"data\", \"length\".", field_name);
                }
                break;
            case Token_Kind::Vec2:
                if (field_name != String_View_literal("x") && 
                    field_name != String_View_literal("y")) {
                    type_error(p_object,
                        "does not have field \"{}\", it has fields \"x\", \"y\".", field_name);
                }
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            case Token_Kind::Vec3:
                if (field_name != String_View_literal("x") && 
                    field_name != String_View_literal("y") &&
                    field_name != String_View_literal("z")) {
                    type_error(p_object,
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
                    type_error(p_object,
                        "does not have field \"{}\", it has fields \"x\", \"y\", \"z\", \"w\".", field_name);
                }
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::F32;
                break;
            default:
                type_error(p_object, "does not have fields to access.");
            }
        } break;
        case Type_Annotation_Kind::Slice: {
            if (field_name == String_View_literal("data")) {
                Type_Annotation *slice_subtype = p_object->p_type_annotation->slice.p_annotation;
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Pointer;
                p_expr->p_type_annotation->pointer.p_annotation = slice_subtype;
            } else if (field_name == String_View_literal("length")) {
                p_expr->p_type_annotation->kind = Type_Annotation_Kind::Builtin;
                p_expr->p_type_annotation->builtin.keyword = Token_Kind::Int;
            } else {
                type_error(p_object,
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
            *p_expr->p_type_annotation = p_object->p_type_annotation->tuple.types[field_as_number];
        } break;
        case Type_Annotation_Kind::UserDefined: {
            String_View type_name = p_object->p_type_annotation->user_defined.name;
            if (type_name.length == 0) {
                error_at(*p_lexer, p_expr->location, "Accessing annonymous structs is invalid.");
            }
            if (p_parser->struct_definitions.contains(type_name)) {
                const Struct_Definition &def = p_parser->struct_definitions[type_name];
                if (!def.contains(field_name)) {
                    type_error(p_object, "does not have a field called \"{}\".", field_name);
                }
            } else if (p_parser->union_definitions.contains(type_name)) {
                const Union_Definition &def = p_parser->union_definitions[type_name];
                if (!def.contains(field_name)) {
                    type_error(p_object, "does not have a field called \"{}\".", field_name);
                }
            } else {
                type_error(p_object, "is not a defined struct or union.");
            }
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::UserDefined;
            p_expr->p_type_annotation->user_defined.name = type_name;
        } break;
        default:
            type_error(p_object, "does not have fields to access.");
        }
        feature_todo(*p_lexer, p_expr->location, "type-resolving field access expressions");
    } break;

    case Expr_Kind::Variable: {
        String_View name = p_expr->variable.name;
        if (p_parser->function_definitions.contains(name)) {
            p_expr->p_type_annotation = p_parser->type_annotation_pool.append();
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Function;
            p_expr->p_type_annotation->function.name = name;
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
        } break;
        case Initializer_List_Kind::Unnamed: {
            // Checking that all elements have the same type is done later when this is used.
            for (Expr *p_array_element : p_expr->initializer_list.unnamed.expressions) {
                resolve_expr_tree(p_parser, symbol_table, p_array_element);
            }
            p_expr->p_type_annotation->kind = Type_Annotation_Kind::Array;
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

#undef type_error
}

void Type_Checker::resolve_global_variable_types()
{
    Symbol_Table symbol_table = {};

    for (const auto &[name, def] : p_parser->global_variable_definitions) {
        assert(def.p_initializer);
        resolve_expr_tree(p_parser, symbol_table, def.p_initializer);
        symbol_table[name] = def.p_initializer->p_type_annotation;
        // TODO: Checking that the type of the initializer is compatible with
        //       the type of the variable definition (casting if it is a number).
    }
}

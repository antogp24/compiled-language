#include "type_checker.h"
#include "core/dynamic_stack.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>

void Type_Checker::check()
{
    check_user_defined_type_completeness();
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
using Sized_Type_Graph = std::unordered_map<std::string, std::vector<Sized_Type_Node>>;

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
        Stack<Sized_Type_Node> to_visit = {};
        to_visit.push(start_node);
        while (!to_visit.is_empty()) {
            Sized_Type_Node node = to_visit.pop();
            if (visited.contains(node)) {
                error_at(*this->p_parser->p_lexer, node.location,
                    "{} is an incomplete type. Consider adding type indirection, for example: *{}",
                    node.name, node.name);
            } else {
                visited.insert(node);
                for (const Sized_Type_Node &neighbor : graph.at(node.name.to_std_string())) {
                    to_visit.push(neighbor);
                }
            }
        }
        to_visit.destroy();
    };
    for (const auto &[_, struct_def] : p_parser->struct_definitions) {
        dfs_check_cycle(graph, { .name = struct_def.name, .location = struct_def.location });
    }
    for (const auto &[_, union_def] : p_parser->union_definitions) {
        dfs_check_cycle(graph, { .name = union_def.name, .location = union_def.location });
    }
}

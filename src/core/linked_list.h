#pragma once

#include "assertions.h"

#include <memory>
#include <iterator>

template <typename T, typename Allocator = std::allocator<T>>
struct Singly_Linked_List {
    struct Node {
        T data;
        Node *next = nullptr;

        Node(const T &value) : data(value) {}
    };
    using Node_Allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using Alloc_Traits = std::allocator_traits<Node_Allocator>;
public:
    Node *head = nullptr;
    Node *tail = nullptr;
private:
    [[no_unique_address]] Node_Allocator allocator;

public:
    Singly_Linked_List() = default;
    explicit Singly_Linked_List(const Node_Allocator& alloc) : allocator(alloc) {}

    ~Singly_Linked_List()
    {
        Node *current = head;
        while (current) {
            Node *next = current->next;
            Alloc_Traits::destroy(allocator, current);
            Alloc_Traits::deallocate(allocator, current, 1);
            current = next;
        }
    }

    constexpr bool is_empty() const
    {
        return head == nullptr;
    }

    constexpr const T &get_last() const { Assert(tail); return tail->data; }
    constexpr       T &get_last()       { Assert(tail); return tail->data; }

    void append(const T &element)
    {
        Node *new_node = Alloc_Traits::allocate(allocator, 1);
        Alloc_Traits::construct(allocator, new_node, element);
        if (head == nullptr) {
            head = new_node;
            tail = new_node;
        } else {
            tail->next = new_node;
            tail = new_node;
        }
    }

    Node *at(size_t index) const
    {
        size_t i = 0;
        for (Node *node = head; node != nullptr; node = node->next) {
            if (index == i) {
                return node;
            }
            i++;
        }
        return nullptr;
    }

    struct Iterator {
        Node *current;

        using iterator_category = std::forward_iterator_tag;

        Iterator(Node *node) : current(node) {}

        T &operator*() const
        {
            return current->data;
        }

        Iterator &operator++()
        {
            if (current) {
                current = current->next;
            }
            return *this;
        }

        bool operator==(const Iterator &other) const
        {
            return current == other.current;
        }

        bool operator!=(const Iterator &other) const
        {
            return current != other.current;
        }
    };

    Iterator begin()
    {
        return Iterator(head);
    }

    Iterator end()
    {
        return Iterator(nullptr);
    }
};

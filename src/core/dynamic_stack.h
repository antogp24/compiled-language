#pragma once

#include <malloc.h>
#include "assertions.h"

template <typename T>
struct Stack {
    T *items;
    size_t count;
    size_t capacity;

    static constexpr float GROWTH_FACTOR = 1.5f;
    static constexpr size_t DEFAULT_CAPACITY = 8;

    void destroy()
    {
        if (items) {
            free(items);
            items = nullptr;
        }
    }

    constexpr bool is_empty() const { return count == 0; }

    void resize_if_needed(size_t added_element_count)
    {
        if (count + added_element_count > capacity) {
            if (capacity == 0) {
                capacity = DEFAULT_CAPACITY;
                // This makes it so that realloc acts like malloc the first time something is appended.
                debug_assert(items == nullptr); 
            }
            while (count + added_element_count > capacity) {
                debug_assert(capacity < (size_t)(capacity * GROWTH_FACTOR));
                capacity = (size_t)(capacity * GROWTH_FACTOR);
            }
            items = (T*)realloc(items, capacity * sizeof(T));
            debug_assert(items != nullptr);
        }
    }

    void push(const T &element)
    {
        resize_if_needed(1);
        items[count++] = element;
    }

    T pop()
    {
        if (count == 0) {
            panic("Stack underflow.");
        }
        T removed = items[count - 1];
        --count;
        return removed;
    }

    // Use this type to have instead to have RAII.
    struct Scoped : Stack { ~Scoped() { this->destroy(); } };
};

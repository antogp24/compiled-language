#pragma once

// I prefer making my equivalent of std::vector
// because std::vector variables show up horrible on the debugger.
// I hate msvc's implementation of it.

#include <malloc.h>
#include "assertions.h"

// Unlike std::vector, it does not use RAII.
// It explicitly expects the user to call the destroy function.
template <typename T>
struct Dynamic_Array {
    T *items;
    size_t count;
    size_t capacity;

    static constexpr float GROWTH_FACTOR = 1.5f;
    static constexpr size_t DEFAULT_CAPACITY = 8;

    constexpr T *data() const { return items; }
    constexpr bool is_empty() const { return count == 0; }

    constexpr const T &operator[](size_t index) const { debug_assert(index < count); return items[index]; }
    constexpr       T &operator[](size_t index)       { debug_assert(index < count); return items[index]; }

    constexpr const T &get_last() const { return operator[](count - 1); }
    constexpr       T &get_last()       { return operator[](count - 1); }

    void reserve(size_t initial_capacity)
    {
        debug_assert(count == 0 && capacity == 0);
        debug_assert(initial_capacity > 0);

        capacity = initial_capacity;
        items = (T*)calloc(initial_capacity, sizeof(T));
    }

    void resizeIfNeeded(size_t added_element_count)
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

    void append(const T &element)
    {
        resizeIfNeeded(1);
        items[count++] = element;
    }

    void append(T &&element)
    {
        resizeIfNeeded(1);
        items[count++] = element;
    }

    void destroy()
    {
        if (items) {
            free(items);
            items = nullptr;
        }
    }
};

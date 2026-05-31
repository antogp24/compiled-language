#pragma once

#include "assertions.h"

#include <format>
#include <string>
#include <string_view>
#include <type_traits>

// I had to create this because std::string_view is not fucking
// considered a trivial type, according to std::is_trivial_v.

struct String_View {
    const char *data;
    size_t length;

    // Returns a substring in the range [start, end)
    constexpr String_View slice(size_t start, size_t end)
    {
        if ((start > end) || (end > length)) {
            return {};
        }
        return String_View{ data + start, end - start };
    }

    constexpr const char& operator[](size_t index) const
    {
        debug_assert(index < length);
        return data[index];
    }

    constexpr int compare(String_View other) const
    {
        size_t i = 0;
        for (; i < this->length && i < other.length; ++i) {
            if (this->data[i] != other.data[i]) {
                break;
            }
        }
        int a = (this->data && i < this->length) ? this->data[i] : 0;
        int b = (other.data && i < other.length) ? other.data[i] : 0;
        return a - b;
    }

    constexpr bool equals(String_View other) const
    {
        if (length != other.length) {
            return false;
        }
        for (size_t i = 0; i < length; ++i) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }

    constexpr bool operator==(String_View other) const
    {
        return this->equals(other);
    }

    constexpr bool operator<(String_View other) const
    {
        return this->compare(other) < 0;
    }

    constexpr bool operator>(String_View other) const
    {
        return this->compare(other) > 0;
    }

    std::string to_std_string() const
    {
        return std::string(data, length);
    }

    static String_View from_cstr(const char *cstr)
    {
        return { cstr, strlen(cstr) };
    }
};
static_assert(std::is_aggregate_v<String_View>);

#define String_View_literal(string_literal)\
    String_View{ .data = (string_literal), .length = sizeof(string_literal) - 1 }

template <>
struct std::formatter<String_View> {
    constexpr auto parse(std::format_parse_context &ctx)
    {
        return ctx.begin();
    }

    auto format(const String_View &str, std::format_context &ctx) const
    {
        return std::format_to(ctx.out(), "{}", std::string_view{str.data, str.length});
    }
};

template<>
struct std::hash<String_View> {
    size_t operator()(const String_View &str) const
    {
        return std::hash<std::string_view>{}(std::string_view{str.data, str.length});
    }
};

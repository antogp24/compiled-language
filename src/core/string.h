#pragma once

#include "assertions.h"

#include <format>
#include <string>
#include <string_view>

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
        Assert(index < length);
        return data[index];
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

    std::string to_std_string() const
    {
        return std::string(data, length);
    }

    static String_View from_cstr(const char *cstr)
    {
        return { cstr, strlen(cstr) };
    }
};

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


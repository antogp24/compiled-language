#pragma once

// Fuck <optional>

#include <utility>
#include "assertions.h"

template <typename T>
struct Option {
    T value;
    bool has_value;

    constexpr bool is_some() const { return has_value; }
    constexpr bool is_none() const { return !has_value; }

    constexpr operator bool() const { return has_value; }

    constexpr const T &unwrap() const & { Assert(has_value); return value; }
    constexpr       T &unwrap()       & { Assert(has_value); return value; }

    constexpr const T &&unwrap() const && { Assert(has_value); return std::move(value); }
    constexpr       T &&unwrap()       && { Assert(has_value); return std::move(value); }

    constexpr bool operator==(const Option<T> &other) const {
        return this->value == other.value;
    }
};

#define Some(x) Option{ x, true }
#define None(T) Option<T>{}


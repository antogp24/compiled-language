#pragma once

#include <unordered_map>
#include <vector>

// This is just a wrapper of std::unordered_map that retains
// the order of the elements with a std::vector of keys.
// 
// This is super important for global variables.
//
// It benefits from O(1) element lookup, and preservation of order.
// It unfortunately probably uses a lot of memory.
template <typename K, typename V>
struct Ordered_Map {
    std::unordered_map<K, V> map;
    std::vector<K> order;

    void insert(const K &key, const V &value)
    {
        order.push_back(key);
        map[key] = value;
    }
};


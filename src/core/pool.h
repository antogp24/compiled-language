#pragma once

#include "linked_list.h"

template <typename T, size_t Block_Item_Count>
struct Pool {
    struct Block {
        T items[Block_Item_Count];
        uint32_t item_count;

        constexpr bool is_full() const { return item_count == Block_Item_Count; }
        constexpr bool in_bounds(uint32_t index) const { return index < item_count; }

        constexpr const T &operator[](uint32_t index) const { Assert(in_bounds(index)); return items[index]; }
        constexpr       T &operator[](uint32_t index)       { Assert(in_bounds(index)); return items[index]; }
    };
    using Node = Singly_Linked_List<Block>::Node;
    struct ID {
        Node *block_node;
        uint32_t index_inside_block;

        constexpr bool operator==(const ID &other) const
        {
            return (block_node == other.block_node) && (index_inside_block == other.index_inside_block);
        }
    };
public:
    Singly_Linked_List<Block> blocks;

    constexpr const T &get(ID id) const { Assert(id.block_node); return id.block_node->data[id.index_inside_block]; }
    constexpr       T &get(ID id)       { Assert(id.block_node); return id.block_node->data[id.index_inside_block]; }

    constexpr T *get_ptr(ID id) const {
        Assert(id.block_node);
        Block &block = id.block_node->data;
        Assert(block.in_bounds(id.index_inside_block));
        return &block.items[id.index_inside_block]; 
    }

    Block &get_block_with_available_space()
    {
        if (blocks.is_empty() || blocks.get_last().is_full()) {
            blocks.append(Block{});
        }
        return blocks.get_last();
    }

    T* append()
    {
        Block &block = get_block_with_available_space();
        uint32_t index_inside_block = block.item_count;
        block.item_count++;
        return &block.items[index_inside_block];
    }

    // Required to be able to use this type as a custom allocator.
    T *allocate(size_t n)
    {
        Assert(n == 1);
        return append();
    }

    // Required to be able to use this type as a custom allocator.
    void deallocate(T *p, size_t n)
    {
        (void)p;
        (void)n;
        // Does nothing because all the elements have the same lifetime.
    }

    struct Iterator {
        ID id = {};

        Iterator(ID identifier) : id(identifier) {}

        T &operator*()
        {
            Assert(id.block_node != nullptr);
            Block &block = id.block_node->data;
            return block[id.index_inside_block];
        }

        Iterator &operator++()
        {
            Assert(id.block_node != nullptr);
            Block &block = id.block_node->data;
            if (block.is_full()) {
                id.block_node = id.block_node->next;
                id.index_inside_block = 0;
            } else {
                id.index_inside_block++;
            }
            return *this;
        }

        constexpr bool operator!=(const Iterator &other) const
        {
            return id != other.id;
        }
    };

    Iterator begin()
    {
        return Iterator{ ID{ .block_node = blocks.head, .index_inside_block = 0 } };
    }

    Iterator end()
    {
        return Iterator{ ID{ .block_node = blocks.tail, .index_inside_block = blocks.get_last().item_count }};
    }
};

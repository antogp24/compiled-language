#pragma once

#include "dynamic_array.h"

struct Pool_ID {
    uint32_t block_index;
    uint32_t index_inside_block;

    constexpr bool operator==(const Pool_ID &other) const
    {
        return (block_index == other.block_index) && (index_inside_block == other.index_inside_block);
    }
};

template <typename T, size_t Block_Item_Count>
struct Pool {
    struct Block {
        T items[Block_Item_Count];
        uint32_t item_count;

        constexpr bool is_full() const { return item_count == Block_Item_Count; }

        constexpr const T &operator[](uint32_t index) const { Assert(index < item_count); return items[index]; }
        constexpr       T &operator[](uint32_t index)       { Assert(index < item_count); return items[index]; }
    };
    Dynamic_Array<Block> blocks;

    constexpr const T &get(Pool_ID id) const { return blocks[id.block_index][id.index_inside_block]; }
    constexpr       T &get(Pool_ID id)       { return blocks[id.block_index][id.index_inside_block]; }

    struct Iterator {
        Pool_ID id = {};
        Dynamic_Array<Block>& blocks;

        Iterator(Pool_ID indices, Dynamic_Array<Block>& blocksRef)
            : id(indices), blocks(blocksRef) {}

        T &operator*()
        {
            Block &block = blocks[id.block_index];
            return block[id.index_inside_block];
        }

        Iterator &operator++()
        {
            if (blocks[id.block_index].is_full()) {
                id.block_index++;
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
        Pool_ID id = { .block_index = 0, .index_inside_block = 0 };
        return Iterator{ id, blocks };
    }

    Iterator end()
    {
        Pool_ID id = {
            .block_index = (uint32_t)blocks.count - 1,
            .index_inside_block = blocks.get_last().item_count,
        };
        return Iterator{ id, blocks };
    }

    uint32_t getBlockWithAvailableSpace()
    {
        if (blocks.is_empty()) {
            blocks.reserve(2);
            blocks.count = 1;
            return 0;
        }
        const Block &last = blocks.get_last();
        if (last.is_full()) {
            blocks.append(Block{ .items = {}, .item_count = 0 });
        }
        return (uint32_t)blocks.count - 1;
    }

    Pool_ID allocate()
    {
        uint32_t block_index = getBlockWithAvailableSpace();
        Block &block = blocks[block_index];
        uint32_t index_inside_block = block.item_count;
        block.item_count++;
        return { block_index, index_inside_block };
    }
};

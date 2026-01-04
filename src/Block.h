#pragma once
#include <cstdint>

enum class BlockType : uint8_t {
    Air = 0,
    Dirt = 1,
    Stone = 2,
    Wood = 3,
    Leaves = 4,
    Water = 5,
    Torch = 6,
    Count
};

struct BlockFace {
    enum Face : uint8_t {
        Front = 0,
        Back = 1,
        Left = 2,
        Right = 3,
        Top = 4,
        Bottom = 5
    };
};

struct Block {
    BlockType type;

    Block() : type(BlockType::Air) {}
    explicit Block(BlockType t) : type(t) {}

    bool IsAir() const { return type == BlockType::Air; }
    bool IsTransparent() const { return type == BlockType::Air || type == BlockType::Water || type == BlockType::Torch; }
    bool IsLiquid() const { return type == BlockType::Water; }
    bool IsLightSource() const { return type == BlockType::Torch; }
};

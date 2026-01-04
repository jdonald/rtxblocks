#pragma once
#include "Block.h"
#include "MathUtils.h"
#include <array>

struct BlockProperties {
    Vector4 color;
    float lightEmission;
    bool isTransparent;
    bool isSolid;
    const char* name;
};

class BlockDatabase {
public:
    static void Initialize();
    static const BlockProperties& GetProperties(BlockType type);

private:
    static std::array<BlockProperties, static_cast<size_t>(BlockType::Count)> s_properties;
    static bool s_initialized;
};

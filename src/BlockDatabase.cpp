#include "BlockDatabase.h"

std::array<BlockProperties, static_cast<size_t>(BlockType::Count)> BlockDatabase::s_properties;
bool BlockDatabase::s_initialized = false;

void BlockDatabase::Initialize() {
    if (s_initialized) return;

    // Air
    s_properties[static_cast<size_t>(BlockType::Air)] = {
        Vector4(0, 0, 0, 0),
        0.0f,
        true,
        false,
        "Air"
    };

    // Dirt - brown color
    s_properties[static_cast<size_t>(BlockType::Dirt)] = {
        Vector4(0.6f, 0.4f, 0.2f, 1.0f),
        0.0f,
        false,
        true,
        "Dirt"
    };

    // Stone - gray color
    s_properties[static_cast<size_t>(BlockType::Stone)] = {
        Vector4(0.5f, 0.5f, 0.5f, 1.0f),
        0.0f,
        false,
        true,
        "Stone"
    };

    // Wood - brown-orange color
    s_properties[static_cast<size_t>(BlockType::Wood)] = {
        Vector4(0.6f, 0.3f, 0.1f, 1.0f),
        0.0f,
        false,
        true,
        "Wood"
    };

    // Leaves - green color
    s_properties[static_cast<size_t>(BlockType::Leaves)] = {
        Vector4(0.2f, 0.8f, 0.2f, 0.7f),
        0.0f,
        true,
        true,
        "Leaves"
    };

    // Water - blue color with transparency
    s_properties[static_cast<size_t>(BlockType::Water)] = {
        Vector4(0.2f, 0.4f, 0.9f, 0.6f),
        0.0f,
        true,
        false,
        "Water"
    };

    // Torch - yellow/orange light source
    s_properties[static_cast<size_t>(BlockType::Torch)] = {
        Vector4(1.0f, 0.8f, 0.3f, 1.0f),
        14.0f,
        true,
        false,
        "Torch"
    };

    s_initialized = true;
}

const BlockProperties& BlockDatabase::GetProperties(BlockType type) {
    if (!s_initialized) {
        Initialize();
    }
    return s_properties[static_cast<size_t>(type)];
}

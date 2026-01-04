cmake_minimum_required(VERSION 3.15)
project(RTXBlocks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Source files
set(SOURCES
    src/main.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/World.cpp
    src/Chunk.cpp
    src/Player.cpp
    src/Mob.cpp
    src/TerrainGenerator.cpp
    src/PerlinNoise.cpp
    src/BlockDatabase.cpp
)

set(HEADERS
    src/Window.h
    src/Renderer.h
    src/Camera.h
    src/World.h
    src/Chunk.h
    src/Block.h
    src/Player.h
    src/Mob.h
    src/TerrainGenerator.h
    src/PerlinNoise.h
    src/BlockDatabase.h
    src/MathUtils.h
)

# Shader files
set(SHADERS
    shaders/BlockVertex.hlsl
    shaders/BlockPixel.hlsl
    shaders/ShadowVertex.hlsl
    shaders/ShadowPixel.hlsl
    shaders/UIVertex.hlsl
    shaders/UIPixel.hlsl
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Windows-specific settings
if(WIN32)
    # DirectX libraries
    target_link_libraries(${PROJECT_NAME}
        d3d11
        dxgi
        d3dcompiler
        dxguid
        winmm
    )

    # Set subsystem to Windows (GUI app)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Copy shaders to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders
)

# Installation
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY shaders DESTINATION bin)

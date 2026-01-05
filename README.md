# RTXBlocks - Minecraft-Inspired Block Demo

A Minecraft-inspired voxel block demo built in C++ using DirectX 11, featuring creative mode gameplay with procedurally generated terrain.

![rasterization screenshot](screenshot.png)

Despite the repo name, DXR-based raytracing mode (press K) does not work yet, but we're getting there.

## Features

### Gameplay
- **Creative Mode**: Fly freely and build without limitations
- **Block Types**: Dirt, Stone, Wood, Tree Leaves, Water, Air, and Illuminating Torches
- **Procedurally Generated Terrain**: Hills, trees, and water bodies using Perlin noise
- **Block Placement/Breaking**: Left-click to break, right-click to place blocks
- **Inventory System**: Single hotbar with 9 slots (use number keys 1-9 to select)
- **Camera Modes**: Toggle between first-person, third-person back, and third-person front views with F5
- **Mob System**: Passive cow mobs that wander around

### Graphics
- **DirectX 11 Rendering**: Efficient rasterization-based rendering
- **Soft Shadows**: Percentage Closer Filtering (PCF) for smooth shadow edges
- **Chunk-based World**: Efficient rendering with dynamic chunk loading/unloading
- **Lighting System**: Directional lighting with ambient and diffuse components
- **Raytracing Stub**: Framework in place for future DXR implementation

### Controls
- **WASD**: Move horizontally
- **Space**: Fly up
- **Shift**: Fly down
- **Mouse**: Look around
- **Left Click**: Break block
- **Right Click**: Place block
- **1-9 Keys**: Select inventory slot
- **F5**: Toggle camera perspective
- **R**: Toggle render mode (raytracing stubbed)
- **ESC**: Toggle mouse capture

## Building

### Prerequisites

#### Windows (MSYS2)
1. Install [MSYS2](https://www.msys2.org/)
2. Open MSYS2 MinGW 64-bit terminal
3. Install required packages:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
```

#### Linux (Cross-compilation)
Install mingw-w64 for cross-compilation:

**Ubuntu/Debian:**
```bash
sudo apt-get install mingw-w64 cmake ninja-build
```

**Fedora:**
```bash
sudo dnf install mingw64-gcc mingw64-gcc-c++ cmake ninja-build
```

**Arch:**
```bash
sudo pacman -S mingw-w64-gcc cmake ninja
```

### Build Instructions

#### Windows (MSYS2)
```bash
mkdir build
cd build
cmake -G "Ninja" ..
ninja
```

The executable will be in `build/bin/RTXBlocks.exe`

#### Linux (Cross-compilation)
```bash
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=../toolchain-mingw.cmake ..
ninja
```

Create a toolchain file `toolchain-mingw.cmake` in the project root:
```cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

### Running

```bash
cd build/bin
./RTXBlocks.exe
```

The shaders directory will be automatically copied to the build directory.

## Architecture

### Core Systems

#### World Management
- **Chunk System**: 16x256x16 voxel chunks with automatic mesh generation
- **Terrain Generator**: Multi-octave Perlin noise for natural-looking landscapes
- **Dynamic Loading**: Chunks load/unload based on player position
- **Mesh Optimization**: Only visible block faces are rendered

#### Rendering Pipeline
1. **Shadow Pass**: Render depth map for shadow mapping (stubbed)
2. **Main Pass**: Render world geometry with lighting and shadows
3. **Mob Pass**: Render animated entities
4. **UI Pass**: Render inventory and HUD

#### Block System
- **Block Database**: Centralized properties (color, transparency, light emission)
- **Block Types**: Enum-based type system for easy extension
- **Transparent Blocks**: Special handling for water and leaves

#### Player System
- **Movement**: Free-flying creative mode with WASD + Space/Shift
- **Camera**: Three perspective modes (F5 toggles)
- **Block Interaction**: Raycasting for accurate block selection
- **Inventory**: Hotbar with all available block types

#### Mob System
- **Simple AI**: Wandering behavior with random target selection
- **Mesh Generation**: Procedural box-based models
- **Animation**: Position interpolation for smooth movement

### Shader Architecture

#### Block Shaders (BlockVertex.hlsl / BlockPixel.hlsl)
- Transforms vertices to world, view, and projection space
- Calculates lighting (ambient + diffuse)
- Implements PCF soft shadows using comparison sampling
- Supports per-block coloring

#### Shadow Shaders (ShadowVertex.hlsl / ShadowPixel.hlsl)
- Renders to shadow map depth buffer
- Light-space transformation

#### UI Shaders (UIVertex.hlsl / UIPixel.hlsl)
- Simple 2D rendering in screen space
- Vertex colors for UI elements

## Future Enhancements

### Planned Features
- **DirectX Raytracing (DXR)**: Hardware-accelerated raytracing mode
  - Global illumination
  - Realistic reflections and refractions
  - Real-time path tracing
- **More Block Types**: Grass, sand, glass, colored blocks
- **More Mobs**: Sheep, pigs, chickens with varied AI
- **Sound System**: Ambient sounds and block placement/breaking sounds
- **Save/Load**: World persistence
- **Multiplayer**: Network support for cooperative building

### Raytracing Stub
The renderer includes a stubbed raytracing mode that can be enabled by pressing 'R'. The infrastructure is in place for DXR integration:
- Render mode enumeration
- Shader resource management
- Acceleration structure placeholders

To implement DXR:
1. Upgrade to DirectX 12
2. Create bottom-level acceleration structures (BLAS) for chunks
3. Create top-level acceleration structure (TLAS) for world
4. Implement ray generation, closest hit, and miss shaders
5. Add denoising for real-time performance

## Technical Details

- **Language**: C++17
- **Graphics API**: DirectX 11
- **Build System**: CMake + Ninja
- **Compiler**: MinGW-w64 GCC
- **Platform**: Windows (cross-platform build support)
- **Dependencies**: Windows SDK (DirectX libraries)

## License

This project is a demonstration and educational resource.

## Credits

Inspired by Minecraft's creative mode and voxel rendering techniques

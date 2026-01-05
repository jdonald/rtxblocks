#include "Window.h"
#include "Renderer.h"
#include "World.h"
#include "Player.h"
#include "Mob.h"
#include "BlockDatabase.h"
#include "SoundSystem.h"
#include "DX12Renderer.h"
#include <windows.h>
#include <chrono>
#include <vector>
#include <memory>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize block database
    BlockDatabase::Initialize();

    // Create window
    Window window(1280, 720, "RTXBlocks - Minecraft-inspired Block Demo");
    if (!window.Create()) {
        MessageBox(nullptr, "Failed to create window", "Error", MB_OK);
        return -1;
    }

    // Create renderer
    Renderer renderer;
    if (!renderer.Initialize(&window)) {
        MessageBox(nullptr, "Failed to initialize renderer", "Error", MB_OK);
        return -1;
    }

    DX12Renderer dx12Renderer;
    bool dx12Ready = dx12Renderer.Initialize(&window);

    // Create sound system
    SoundSystem soundSystem;
    soundSystem.Initialize();

    // Create world
    World world(12345);

    // Create player - spawn above terrain
    Player player;
    int spawnHeight = world.GetTerrainHeight(0, 0);
    float spawnY = static_cast<float>(spawnHeight + 10);
    if (spawnY > CHUNK_HEIGHT - 2) {
        spawnY = static_cast<float>(CHUNK_HEIGHT - 2);
    }
    player.SetPosition(Vector3(0, spawnY, 0));
    player.CreateMesh(renderer.GetDevice());

    // Create some cows
    std::vector<std::unique_ptr<Mob>> mobs;
    for (int i = 0; i < 5; i++) {
        auto cow = std::make_unique<Mob>(MobType::Cow,
                                         Vector3(i * 10.0f - 20.0f, 70.0f, 20.0f));
        cow->CreateMesh(renderer.GetDevice());
        mobs.push_back(std::move(cow));
    }

    // Show window
    window.Show();
    bool mouseCaptured = true;
    window.SetMouseCapture(mouseCaptured);

    // Main loop
    auto lastTime = std::chrono::high_resolution_clock::now();
    bool running = true;

    // FPS calculation
    float fpsAccumulator = 0.0f;
    int frameCount = 0;
    float currentFPS = 60.0f;
    int lastWidth = window.GetWidth();
    int lastHeight = window.GetHeight();

    while (running) {
        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Cap delta time to avoid huge jumps
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Update FPS counter
        fpsAccumulator += deltaTime;
        frameCount++;
        if (fpsAccumulator >= 0.5f) {
            currentFPS = static_cast<float>(frameCount) / fpsAccumulator;
            fpsAccumulator = 0.0f;
            frameCount = 0;
        }

        // Process window messages
        if (!window.ProcessMessages()) {
            running = false;
            break;
        }

        // Toggle mouse capture with ESC
        if (window.WasKeyPressed(VK_ESCAPE)) {
            mouseCaptured = !mouseCaptured;
            window.SetMouseCapture(mouseCaptured);
        }

        // Toggle debug HUD with F3
        if (window.WasKeyPressed(VK_F3)) {
            renderer.ToggleDebugHUD();
        }

        // Toggle fullscreen with F11
        if (window.WasKeyPressed(VK_F11)) {
            window.ToggleFullscreen();
        }

        // Toggle render mode (stubbed raytracing)
        if (window.WasKeyPressed('K')) {
            if (renderer.GetRenderMode() == RenderMode::Rasterization) {
                renderer.SetRenderMode(RenderMode::Raytracing);
            } else {
                renderer.SetRenderMode(RenderMode::Rasterization);
            }
        }

        // Update player
        player.Update(deltaTime, &window, &world, &soundSystem);

        // Update camera aspect ratio
        player.GetCamera().SetAspectRatio(
            static_cast<float>(window.GetWidth()) /
            static_cast<float>(window.GetHeight())
        );

        int width = window.GetWidth();
        int height = window.GetHeight();
        if (width != lastWidth || height != lastHeight) {
            if (dx12Ready) {
                dx12Renderer.Resize(static_cast<UINT>(width), static_cast<UINT>(height));
            }
            renderer.Resize(width, height);
            lastWidth = width;
            lastHeight = height;
        }

        // Update world (load/unload chunks)
        world.Update(player.GetPosition(), renderer.GetDevice());

        // Update mobs
        for (auto& mob : mobs) {
            mob->Update(deltaTime);
        }

        // Render
        bool usedDx12 = false;
        if (dx12Ready) {
            DX12Renderer::UIInfo uiInfo;
            uiInfo.fps = currentFPS;
            uiInfo.showDebug = renderer.IsDebugHUDVisible();
            uiInfo.selectedSlot = player.GetSelectedSlot();

            Vector3 hitPos, hitNormal;
            Block hitBlock;
            Camera& cam = player.GetCamera();
            if (world.Raycast(cam.GetPosition(), cam.GetForward(), 50.0f, hitPos, hitNormal, hitBlock)) {
                uiInfo.hasLookedAtBlock = true;
                uiInfo.lookedAtBlockName = BlockDatabase::GetProperties(hitBlock.type).name;
            }

            if (renderer.GetRenderMode() == RenderMode::Raytracing) {
                usedDx12 = dx12Renderer.RenderRaytracing(&world, player.GetCamera(), uiInfo);
            } else {
                std::vector<Mob*> mobPtrs;
                mobPtrs.reserve(mobs.size());
                for (auto& mob : mobs) {
                    mobPtrs.push_back(mob.get());
                }
                usedDx12 = dx12Renderer.RenderRasterization(&world, player.GetCamera(), uiInfo, &player, mobPtrs);
            }
        }

        if (!usedDx12) {
            renderer.BeginFrame();
            renderer.RenderWorld(&world, player.GetCamera());

            // Render mobs
            for (auto& mob : mobs) {
                renderer.RenderMob(mob.get(), player.GetCamera());
            }

            renderer.RenderPlayer(&player, player.GetCamera());

            renderer.RenderUI(&player);

            // Render debug HUD if enabled
            if (renderer.IsDebugHUDVisible()) {
                DebugInfo debugInfo;
                debugInfo.fps = currentFPS;

                World::DebugStats worldStats = world.GetDebugStats();
                debugInfo.loadedChunkCount = worldStats.chunkCount;
                debugInfo.solidIndexCount = worldStats.solidIndexCount;
                debugInfo.transparentIndexCount = worldStats.transparentIndexCount;

                // Raycast to find looked-at block
                Vector3 hitPos, hitNormal;
                Block hitBlock;
                Camera& cam = player.GetCamera();
                if (world.Raycast(cam.GetPosition(), cam.GetForward(), 50.0f, hitPos, hitNormal, hitBlock)) {
                    debugInfo.hasLookedAtBlock = true;
                    debugInfo.lookedAtBlockType = hitBlock.type;
                    debugInfo.lookedAtBlockPos = hitPos;
                } else {
                    debugInfo.hasLookedAtBlock = false;
                }

                // Add DXR status info if available
                if (dx12Ready) {
                    debugInfo.dxrStatus = dx12Renderer.GetRaytracingStatus().c_str();
                    debugInfo.dxrError = dx12Renderer.GetRaytracingLastError().c_str();
                }

                renderer.RenderDebugHUD(debugInfo);
            }

            renderer.EndFrame();
        }
    }

    return 0;
}

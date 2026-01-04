#include "Window.h"
#include "Renderer.h"
#include "World.h"
#include "Player.h"
#include "Mob.h"
#include "BlockDatabase.h"
#include "SoundSystem.h"
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

    // Create sound system
    SoundSystem soundSystem;
    soundSystem.Initialize();

    // Create world
    World world(12345);

    // Create player
    Player player;
    player.SetPosition(Vector3(0, 100, 0));

    // Create some cows
    std::vector<std::unique_ptr<Mob>> mobs;
    for (int i = 0; i < 5; i++) {
        auto cow = std::make_unique<Mob>(MobType::Cow,
                                         Vector3(i * 10.0f - 20.0f, 70.0f, 20.0f));
        cow->CreateMesh(renderer.GetDevice());
        mobs.push_back(std::move(cow));
    }

    // Show window and capture mouse
    window.Show();
    window.SetMouseCapture(true);

    // Main loop
    auto lastTime = std::chrono::high_resolution_clock::now();
    bool running = true;

    while (running) {
        // Calculate delta time
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Cap delta time to avoid huge jumps
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Process window messages
        if (!window.ProcessMessages()) {
            running = false;
            break;
        }

        // Toggle mouse capture with ESC
        if (window.WasKeyPressed(VK_ESCAPE)) {
            static bool mouseCaptured = true;
            mouseCaptured = !mouseCaptured;
            window.SetMouseCapture(mouseCaptured);
        }

        // Toggle render mode (stubbed raytracing)
        if (window.WasKeyPressed('R')) {
            if (renderer.GetRenderMode() == RenderMode::Rasterization) {
                renderer.SetRenderMode(RenderMode::Raytracing);
                MessageBox(nullptr,
                          "Raytracing mode is stubbed for future DXR implementation.\n"
                          "Switching back to rasterization.",
                          "Info", MB_OK);
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

        // Update world (load/unload chunks)
        world.Update(player.GetPosition(), renderer.GetDevice());

        // Update mobs
        for (auto& mob : mobs) {
            mob->Update(deltaTime);
        }

        // Render
        renderer.BeginFrame();
        renderer.RenderWorld(&world, player.GetCamera());

        // Render mobs
        for (auto& mob : mobs) {
            renderer.RenderMob(mob.get(), player.GetCamera());
        }

        renderer.RenderUI(&player);
        renderer.EndFrame();
    }

    return 0;
}

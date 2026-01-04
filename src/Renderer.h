#pragma once
#include "MathUtils.h"
#include "Camera.h"
#include "Block.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

class Window;
class World;
class Player;
class Mob;

struct DebugInfo {
    float fps;
    bool hasLookedAtBlock;
    BlockType lookedAtBlockType;
    Vector3 lookedAtBlockPos;
    int loadedChunkCount;
    uint64_t solidIndexCount;
    uint64_t transparentIndexCount;
    const char* dxrStatus = nullptr;
    const char* dxrError = nullptr;
};

// UI vertex structure for 2D rendering
struct UIVertex {
    Vector2 position;
    Vector4 color;
};

enum class RenderMode {
    Rasterization,
    Raytracing  // Stubbed for future DXR implementation
};

struct ConstantBuffer {
    Matrix4x4 world;
    Matrix4x4 view;
    Matrix4x4 projection;
    Matrix4x4 lightViewProj;
    Vector4 lightDir;
    Vector4 cameraPos;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Initialize(Window* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Resize(int width, int height);

    void RenderWorld(World* world, Camera& camera);
    void RenderMob(Mob* mob, Camera& camera);
    void RenderPlayer(Player* player, Camera& camera);
    void RenderUI(Player* player);
    void RenderDebugHUD(const DebugInfo& debugInfo);

    void SetDebugHUDVisible(bool visible) { m_debugHUDVisible = visible; }
    bool IsDebugHUDVisible() const { return m_debugHUDVisible; }
    void ToggleDebugHUD() { m_debugHUDVisible = !m_debugHUDVisible; }

    void SetRenderMode(RenderMode mode) { m_renderMode = mode; }
    RenderMode GetRenderMode() const { return m_renderMode; }

    ID3D11Device* GetDevice() { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() { return m_context.Get(); }

private:
    bool CreateDeviceAndSwapChain(Window* window);
    bool CreateRenderTargets();
    bool CreateDepthStencil(int width, int height);
    bool CreateShadowMap();
    bool CompileShaders();
    bool CreateConstantBuffers();
    bool CreateSamplerStates();
    bool CreateRasterizerStates();
    bool CreateUIResources();

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;

    ComPtr<ID3D11RenderTargetView> m_backBufferRTV;
    ComPtr<ID3D11Texture2D> m_depthStencilTexture;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    // Shadow mapping
    ComPtr<ID3D11Texture2D> m_shadowMap;
    ComPtr<ID3D11DepthStencilView> m_shadowMapDSV;
    ComPtr<ID3D11ShaderResourceView> m_shadowMapSRV;
    int m_shadowMapSize;

    // Shaders
    ComPtr<ID3D11VertexShader> m_blockVS;
    ComPtr<ID3D11PixelShader> m_blockPS;
    ComPtr<ID3D11VertexShader> m_shadowVS;
    ComPtr<ID3D11PixelShader> m_shadowPS;
    ComPtr<ID3D11VertexShader> m_uiVS;
    ComPtr<ID3D11PixelShader> m_uiPS;

    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11InputLayout> m_uiInputLayout;

    ComPtr<ID3D11Buffer> m_constantBuffer;
    ComPtr<ID3D11Buffer> m_uiConstantBuffer;

    ComPtr<ID3D11SamplerState> m_linearSampler;
    ComPtr<ID3D11SamplerState> m_shadowSampler;

    ComPtr<ID3D11RasterizerState> m_solidRasterizer;
    ComPtr<ID3D11RasterizerState> m_wireframeRasterizer;
    ComPtr<ID3D11RasterizerState> m_uiRasterizer;
    ComPtr<ID3D11BlendState> m_alphaBlendState;
    ComPtr<ID3D11DepthStencilState> m_depthDisabledState;
    ComPtr<ID3D11DepthStencilState> m_depthReadOnlyState;

    // UI rendering
    ComPtr<ID3D11Buffer> m_uiVertexBuffer;
    ComPtr<ID3D11Buffer> m_uiIndexBuffer;

    RenderMode m_renderMode;
    int m_width;
    int m_height;
    bool m_debugHUDVisible;

    // Font rendering
    ComPtr<ID3D11Texture2D> m_fontTexture;
    ComPtr<ID3D11ShaderResourceView> m_fontSRV;
    bool CreateFontResources();
    void DrawText(const std::string& text, float x, float y, float scale, const Vector4& color,
                  std::vector<UIVertex>& vertices, std::vector<uint32_t>& indices);

    void RenderShadowPass(World* world);
    void UpdateConstantBuffer(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj);

    bool CompileShaderFromFile(const std::string& filename, const std::string& entryPoint,
                               const std::string& profile, ID3DBlob** blob);
};

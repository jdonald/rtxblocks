#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "DXRCompat.h"
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <string>
#include "Block.h"
#include "MathUtils.h"

using Microsoft::WRL::ComPtr;

class Window;

class DX12Renderer {
public:
    struct UIVertex {
        Vector2 position;
        Vector4 color;
    };

    struct UIInfo {
        float fps = 0.0f;
        bool showDebug = false;
        bool hasLookedAtBlock = false;
        const char* lookedAtBlockName = nullptr;
        int selectedSlot = 0;
        Vector3 camPos;
        Vector3 camDir;
        const char* renderMode = nullptr;
        // Raytracing debug info
        bool isRaytracing = false;
        uint32_t rtVertexCount = 0;
        uint32_t rtIndexCount = 0;
        uint32_t rtTriangleCount = 0;
        bool rtBlasBuilt = false;
        bool rtTlasBuilt = false;
    };

    DX12Renderer();
    ~DX12Renderer();

    bool Initialize(Window* window);
    void Shutdown();
    void Resize(UINT width, UINT height);

    void BeginFrame();
    void EndFrame();
    bool InitializeRaytracing();
    bool InitializeRasterization();
    bool RenderRaytracing(class World* world, const class Camera& camera, UIInfo& uiInfo);
    bool RenderRasterization(class World* world, const class Camera& camera, const UIInfo& uiInfo,
                             const class Player* player, const std::vector<class Mob*>& mobs);

    ID3D12Device* GetDevice() { return m_device.Get(); }

private:
    bool CreateDevice(Window* window);
    bool CreateCommandObjects();
    bool CreateSwapChain(Window* window);
    bool CreateRenderTargets();
    bool CreateRaytracingPipeline();
    bool CreateRaytracingOutput();
    bool CreateRaytracingResources();
    bool BuildAccelerationStructures(const std::vector<struct Vertex>& vertices,
                                     const std::vector<uint32_t>& indices);
    bool CreateShaderTable();
    void UpdateCameraCB(const class Camera& camera);
    bool EnsureRaytracingDescriptors();
    bool CreateRasterPipeline();
    bool CreateRasterResources();
    void UpdateRasterCB(const class Camera& camera);
    void RecreateDepthBuffer();
    bool CreateUIResources();
    void RenderUIInternal(const UIInfo& uiInfo);
    void DrawTextInternal(const std::string& text, float x, float y, float scale,
                  const Vector4& color, std::vector<UIVertex>& vertices,
                  std::vector<uint32_t>& indices);
    void WaitForGpu();
    void MoveToNextFrame();

    static const UINT kFrameCount = 2;

    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize;
    ComPtr<ID3D12Resource> m_renderTargets[kFrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    HANDLE m_fenceEvent;
    UINT m_frameIndex;
    UINT m_width;
    UINT m_height;

    bool m_raytracingReady;
    bool m_rasterReady;
    ComPtr<ID3D12StateObject> m_rtStateObject;
    ComPtr<ID3D12RootSignature> m_rtGlobalRootSig;
    ComPtr<ID3D12Resource> m_outputTexture;
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
    ComPtr<ID3D12Resource> m_cameraCB;
    ComPtr<ID3D12Resource> m_shaderTable;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_vertexUpload;
    ComPtr<ID3D12Resource> m_indexUpload;
    ComPtr<ID3D12Resource> m_blas;
    ComPtr<ID3D12Resource> m_tlas;
    ComPtr<ID3D12Resource> m_blasScratch;
    ComPtr<ID3D12Resource> m_tlasScratch;
    ComPtr<ID3D12Resource> m_instanceDescBuffer;
    UINT64 m_vertexBufferSize;
    UINT64 m_indexBufferSize;
    UINT64 m_vertexUploadSize;
    UINT64 m_indexUploadSize;
    UINT64 m_blasScratchSize;
    UINT64 m_tlasScratchSize;
    D3D12_RESOURCE_STATES m_vertexBufferState;
    D3D12_RESOURCE_STATES m_indexBufferState;
    bool m_outputInCopySource;
    bool m_lastBlasBuilt;
    bool m_lastTlasBuilt;
    uint32_t m_lastRtVertexCount;
    uint32_t m_lastRtIndexCount;

    ComPtr<ID3D12RootSignature> m_rasterRootSig;
    ComPtr<ID3D12PipelineState> m_rasterPSO;
    ComPtr<ID3D12PipelineState> m_rasterTransparentPSO;
    ComPtr<ID3D12DescriptorHeap> m_rasterCbvHeap;
    ComPtr<ID3D12Resource> m_rasterCB;
    ComPtr<ID3D12Resource> m_rasterVB;
    ComPtr<ID3D12Resource> m_rasterIB;
    UINT64 m_rasterVBSize;
    UINT64 m_rasterIBSize;

    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthBuffer;

    ComPtr<ID3D12RootSignature> m_uiRootSig;
    ComPtr<ID3D12PipelineState> m_uiPSO;
    ComPtr<ID3D12Resource> m_uiVB;
    ComPtr<ID3D12Resource> m_uiIB;
};

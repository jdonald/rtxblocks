#include "DX12Renderer.h"
#include "Window.h"
#include "World.h"
#include "Camera.h"
#include "Chunk.h"
#include "Player.h"
#include "Mob.h"
#include "DxcLoader.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

namespace {
struct CameraCB {
    float origin[3];
    float pad0;
    float forward[3];
    float pad1;
    float right[3];
    float pad2;
    float up[3];
    float pad3;
    float viewport[2];
    float pad4[2];
};

struct RasterCB {
    Matrix4x4 world;
    Matrix4x4 view;
    Matrix4x4 projection;
    Vector4 lightDir;
};

std::string ReadFileToString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::ifstream file2("bin/" + path);
        if (!file2.is_open()) {
            return std::string();
        }
        std::stringstream buffer;
        buffer << file2.rdbuf();
        return buffer.str();
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
} // namespace

static Vector3 TransformPoint(const Matrix4x4& m, const Vector3& v) {
    return Vector3(
        v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3],
        v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3],
        v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2] + m.m[2][3]
    );
}

static Vector3 TransformNormal(const Matrix4x4& m, const Vector3& v) {
    return Vector3(
        v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2],
        v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2],
        v.x * m.m[2][0] + v.y * m.m[2][1] + v.z * m.m[2][2]
    ).normalized();
}

static void AppendMeshTransformed(const std::vector<Vertex>& srcVerts,
                                  const std::vector<uint32_t>& srcIndices,
                                  const Matrix4x4& world,
                                  std::vector<Vertex>& outVerts,
                                  std::vector<uint32_t>& outIndices) {
    if (srcVerts.empty() || srcIndices.empty()) {
        return;
    }

    uint32_t baseIndex = static_cast<uint32_t>(outVerts.size());
    outVerts.reserve(outVerts.size() + srcVerts.size());
    for (const auto& v : srcVerts) {
        Vertex out = v;
        out.position = TransformPoint(world, v.position);
        out.normal = TransformNormal(world, v.normal);
        outVerts.push_back(out);
    }

    outIndices.reserve(outIndices.size() + srcIndices.size());
    for (uint32_t idx : srcIndices) {
        outIndices.push_back(baseIndex + idx);
    }
}

namespace {
static const GUID IID_ID3D12Device5 = {0x8b4f173b, 0x2fea, 0x4b80, {0x8f, 0x58, 0x43, 0x07, 0x19, 0x1a, 0xb9, 0x5d}};
static const GUID IID_ID3D12GraphicsCommandList4 = {0x8754318e, 0xd3a9, 0x4541, {0x98, 0xcf, 0x64, 0x5b, 0x50, 0xdc, 0x48, 0x74}};
static const GUID IID_ID3D12StateObjectProperties = {0xde5fa827, 0x9bf9, 0x4f26, {0x89, 0xff, 0xd7, 0xf5, 0x6f, 0xde, 0x38, 0x60}};
static const GUID IID_ID3D12StateObject = {0x47016943, 0xfca8, 0x4594, {0x93, 0xea, 0xaf, 0x25, 0x8b, 0x55, 0x34, 0x6d}};
}

DX12Renderer::DX12Renderer()
    : m_rtvDescriptorSize(0)
    , m_fenceValue(0)
    , m_fenceEvent(nullptr)
    , m_frameIndex(0)
    , m_width(0)
    , m_height(0)
    , m_raytracingReady(false)
    , m_rasterReady(false)
    , m_vertexBufferSize(0)
    , m_indexBufferSize(0)
    , m_outputInCopySource(false)
    , m_rasterVBSize(0)
    , m_rasterIBSize(0) {
}

DX12Renderer::~DX12Renderer() {
    Shutdown();
}

bool DX12Renderer::Initialize(Window* window) {
    m_width = static_cast<UINT>(window->GetWidth());
    m_height = static_cast<UINT>(window->GetHeight());
    if (!CreateDevice(window)) return false;
    if (!CreateCommandObjects()) return false;
    if (!CreateSwapChain(window)) return false;
    if (!CreateRenderTargets()) return false;
    m_rasterReady = InitializeRasterization();
    m_raytracingReady = InitializeRaytracing();
    return true;
}

void DX12Renderer::Resize(UINT width, UINT height) {
    if (!m_swapChain || width == 0 || height == 0) {
        return;
    }
    if (width == m_width && height == m_height) {
        return;
    }

    WaitForGpu();
    for (UINT i = 0; i < kFrameCount; i++) {
        m_renderTargets[i].Reset();
    }
    m_depthBuffer.Reset();
    m_outputTexture.Reset();
    m_outputInCopySource = false;

    m_width = width;
    m_height = height;

    m_swapChain->ResizeBuffers(kFrameCount, m_width, m_height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    CreateRenderTargets();
    RecreateDepthBuffer();
}

void DX12Renderer::Shutdown() {
    WaitForGpu();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

bool DX12Renderer::CreateDevice(Window* window) {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) {
        return false;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
         factory->EnumAdapters1(adapterIndex, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND;
         ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc = {};
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(m_device.GetAddressOf())))) {
            break;
        }
    }

    if (!m_device) {
        return false;
    }

    return true;
}

bool DX12Renderer::CreateCommandObjects() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf())))) {
        return false;
    }

    if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf())))) {
        return false;
    }

    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())))) {
        return false;
    }

    m_commandList->Close();

    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf())))) {
        return false;
    }

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    return true;
}

bool DX12Renderer::CreateSwapChain(Window* window) {
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.BufferCount = kFrameCount;
    swapDesc.Width = m_width;
    swapDesc.Height = m_height;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    if (FAILED(factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            window->GetHandle(),
            &swapDesc,
            nullptr,
            nullptr,
            swapChain.GetAddressOf()))) {
        return false;
    }

    if (FAILED(factory->MakeWindowAssociation(window->GetHandle(), DXGI_MWA_NO_ALT_ENTER))) {
        return false;
    }

    if (FAILED(swapChain.As(&m_swapChain))) {
        return false;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return true;
}

bool DX12Renderer::CreateRenderTargets() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kFrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())))) {
        return false;
    }

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; i++) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(m_renderTargets[i].GetAddressOf())))) {
            return false;
        }
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    return true;
}

void DX12Renderer::BeginFrame() {
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    float clearColor[4] = { 0.5f, 0.7f, 1.0f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
}

void DX12Renderer::EndFrame() {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0);
    MoveToNextFrame();
}

bool DX12Renderer::InitializeRasterization() {
    if (!CreateRasterPipeline()) return false;
    if (!CreateRasterResources()) return false;
    if (!CreateUIResources()) return false;
    return true;
}

bool DX12Renderer::CreateRasterPipeline() {
    DxcLoader dxc;
    if (!dxc.Initialize()) {
        return false;
    }

    std::string source = ReadFileToString("shaders/DX12Block.hlsl");
    if (source.empty()) {
        return false;
    }

    std::vector<std::wstring> args;
    args.push_back(L"-Zi");
    args.push_back(L"-Qembed_debug");

    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob;
    if (!dxc.CompileToDxil(L"DX12Block.hlsl", source, L"VSMain", L"vs_6_0", args, vsBlob, nullptr)) {
        return false;
    }
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob;
    if (!dxc.CompileToDxil(L"DX12Block.hlsl", source, L"PSMain", L"ps_6_0", args, psBlob, nullptr)) {
        return false;
    }

    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParam.Descriptor.ShaderRegister = 0;
    rootParam.Descriptor.RegisterSpace = 0;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &rootParam;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> sigError;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, sigBlob.GetAddressOf(), sigError.GetAddressOf()))) {
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                             IID_PPV_ARGS(m_rasterRootSig.GetAddressOf())))) {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { layout, 4 };
    psoDesc.pRootSignature = m_rasterRootSig.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState = {};
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState = {};
    psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState = {};
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_rasterPSO.GetAddressOf())))) {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transpDesc = psoDesc;
    transpDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    transpDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transpDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transpDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    transpDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    transpDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    transpDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transpDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    if (FAILED(m_device->CreateGraphicsPipelineState(&transpDesc, IID_PPV_ARGS(m_rasterTransparentPSO.GetAddressOf())))) {
        return false;
    }

    return true;
}

bool DX12Renderer::CreateRasterResources() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rasterCbvHeap.GetAddressOf())))) {
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = (sizeof(RasterCB) + 255) & ~255ULL;
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(m_rasterCB.GetAddressOf())))) {
        return false;
    }

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_rasterCB->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = static_cast<UINT>(cbDesc.Width);
    m_device->CreateConstantBufferView(&cbvDesc, m_rasterCbvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())))) {
        return false;
    }
    RecreateDepthBuffer();

    return true;
}

void DX12Renderer::RecreateDepthBuffer() {
    if (!m_device || !m_dsvHeap) {
        return;
    }

    m_depthBuffer.Reset();

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClear = {};
    depthClear.Format = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth = 1.0f;
    depthClear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES depthHeapProps = {};
    depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(m_device->CreateCommittedResource(&depthHeapProps, D3D12_HEAP_FLAG_NONE, &depthDesc,
                                                 D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear,
                                                 IID_PPV_ARGS(m_depthBuffer.GetAddressOf())))) {
        return;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvView = {};
    dsvView.Format = DXGI_FORMAT_D32_FLOAT;
    dsvView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvView, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

bool DX12Renderer::CreateUIResources() {
    DxcLoader dxc;
    if (!dxc.Initialize()) {
        return false;
    }

    std::string source = ReadFileToString("shaders/DX12UI.hlsl");
    if (source.empty()) {
        return false;
    }

    std::vector<std::wstring> args;
    args.push_back(L"-Zi");
    args.push_back(L"-Qembed_debug");

    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob;
    if (!dxc.CompileToDxil(L"DX12UI.hlsl", source, L"VSMain", L"vs_6_0", args, vsBlob, nullptr)) {
        return false;
    }
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob;
    if (!dxc.CompileToDxil(L"DX12UI.hlsl", source, L"PSMain", L"ps_6_0", args, psBlob, nullptr)) {
        return false;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = 0;
    rootDesc.pParameters = nullptr;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> sigError;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, sigBlob.GetAddressOf(), sigError.GetAddressOf()))) {
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                             IID_PPV_ARGS(m_uiRootSig.GetAddressOf())))) {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { layout, 2 };
    psoDesc.pRootSignature = m_uiRootSig.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState = {};
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState = {};
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState = {};
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_uiPSO.GetAddressOf())))) {
        return false;
    }

    return true;
}

void DX12Renderer::DrawTextInternal(const std::string& text, float x, float y, float scale,
                            const Vector4& color, std::vector<UIVertex>& vertices,
                            std::vector<uint32_t>& indices) {
    static const uint8_t fontData[96][7] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        {0x04,0x04,0x04,0x04,0x00,0x04,0x00},
        {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
        {0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00},
        {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
        {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
        {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
        {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
        {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
        {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
        {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},
        {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
        {0x00,0x00,0x00,0x00,0x04,0x04,0x08},
        {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x04,0x00},
        {0x00,0x01,0x02,0x04,0x08,0x10,0x00},
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
        {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
        {0x00,0x04,0x00,0x00,0x04,0x00,0x00},
        {0x00,0x04,0x00,0x00,0x04,0x04,0x08},
        {0x02,0x04,0x08,0x10,0x08,0x04,0x02},
        {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
        {0x08,0x04,0x02,0x01,0x02,0x04,0x08},
        {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
        {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
        {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
        {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
        {0x00,0x10,0x08,0x04,0x02,0x01,0x00},
        {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
        {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
        {0x08,0x04,0x00,0x00,0x00,0x00,0x00},
        {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},
        {0x10,0x10,0x16,0x19,0x11,0x11,0x1E},
        {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E},
        {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F},
        {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},
        {0x06,0x09,0x08,0x1C,0x08,0x08,0x08},
        {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E},
        {0x10,0x10,0x16,0x19,0x11,0x11,0x11},
        {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},
        {0x02,0x00,0x06,0x02,0x02,0x12,0x0C},
        {0x10,0x10,0x12,0x14,0x18,0x14,0x12},
        {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x00,0x00,0x1A,0x15,0x15,0x11,0x11},
        {0x00,0x00,0x16,0x19,0x11,0x11,0x11},
        {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},
        {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10},
        {0x00,0x00,0x0D,0x13,0x0F,0x01,0x01},
        {0x00,0x00,0x16,0x19,0x10,0x10,0x10},
        {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E},
        {0x08,0x08,0x1C,0x08,0x08,0x09,0x06},
        {0x00,0x00,0x11,0x11,0x11,0x13,0x0D},
        {0x00,0x00,0x11,0x11,0x11,0x0A,0x04},
        {0x00,0x00,0x11,0x11,0x15,0x15,0x0A},
        {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},
        {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},
        {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},
        {0x02,0x04,0x04,0x08,0x04,0x04,0x02},
        {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x08,0x04,0x04,0x02,0x04,0x04,0x08},
        {0x00,0x00,0x08,0x15,0x02,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    };

    const float charWidth = 5.0f * scale;
    const float charHeight = 7.0f * scale;
    const float spacing = 1.0f * scale;
    float cursorX = x;

    for (char c : text) {
        if (c < 32 || c > 127) c = '?';
        int charIndex = c - 32;

        for (int row = 0; row < 7; row++) {
            uint8_t rowData = fontData[charIndex][row];
            for (int col = 0; col < 5; col++) {
                if (rowData & (1 << (4 - col))) {
                    float px = cursorX + col * scale;
                    float py = y + row * scale;

                    float ndcX = (px / m_width) * 2.0f - 1.0f;
                    float ndcY = 1.0f - (py / m_height) * 2.0f;
                    float ndcW = (scale / m_width) * 2.0f;
                    float ndcH = (scale / m_height) * 2.0f;

                    uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
                    vertices.push_back({ Vector2(ndcX, ndcY), color });
                    vertices.push_back({ Vector2(ndcX + ndcW, ndcY), color });
                    vertices.push_back({ Vector2(ndcX + ndcW, ndcY - ndcH), color });
                    vertices.push_back({ Vector2(ndcX, ndcY - ndcH), color });

                    indices.push_back(baseIdx);
                    indices.push_back(baseIdx + 1);
                    indices.push_back(baseIdx + 2);
                    indices.push_back(baseIdx);
                    indices.push_back(baseIdx + 2);
                    indices.push_back(baseIdx + 3);
                }
            }
        }

        cursorX += charWidth + spacing;
    }
}

void DX12Renderer::RenderUIInternal(const UIInfo& uiInfo) {
    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;

    float slotSize = 50.0f;
    float spacing = 10.0f;
    float startX = m_width / 2.0f - (9 * slotSize + 8 * spacing) / 2.0f;
    float startY = m_height - slotSize - 20.0f;

    for (int i = 0; i < 9; i++) {
        float x = startX + i * (slotSize + spacing);
        float y = startY;

        Vector4 color = (i == uiInfo.selectedSlot) ? Vector4(1, 1, 0, 1) : Vector4(0.5f, 0.5f, 0.5f, 0.8f);

        float ndcX = (x / m_width) * 2.0f - 1.0f;
        float ndcY = 1.0f - (y / m_height) * 2.0f;
        float ndcW = (slotSize / m_width) * 2.0f;
        float ndcH = (slotSize / m_height) * 2.0f;

        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ Vector2(ndcX, ndcY), color });
        vertices.push_back({ Vector2(ndcX + ndcW, ndcY), color });
        vertices.push_back({ Vector2(ndcX + ndcW, ndcY - ndcH), color });
        vertices.push_back({ Vector2(ndcX, ndcY - ndcH), color });

        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }

    if (uiInfo.showDebug) {
        Vector4 bgColor(0.0f, 0.0f, 0.0f, 0.5f);
        float bgWidth = 300.0f;
        float bgHeight = 100.0f;
        float bgX = 10.0f;
        float bgY = 10.0f;

        float ndcX = (bgX / m_width) * 2.0f - 1.0f;
        float ndcY = 1.0f - (bgY / m_height) * 2.0f;
        float ndcW = (bgWidth / m_width) * 2.0f;
        float ndcH = (bgHeight / m_height) * 2.0f;

        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ Vector2(ndcX, ndcY), bgColor });
        vertices.push_back({ Vector2(ndcX + ndcW, ndcY), bgColor });
        vertices.push_back({ Vector2(ndcX + ndcW, ndcY - ndcH), bgColor });
        vertices.push_back({ Vector2(ndcX, ndcY - ndcH), bgColor });

        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);

        char fpsText[64];
        sprintf(fpsText, "FPS: %.1f", uiInfo.fps);
        DrawTextInternal(fpsText, 15.0f, 15.0f, 2.0f, Vector4(1, 1, 1, 1), vertices, indices);

        if (uiInfo.hasLookedAtBlock && uiInfo.lookedAtBlockName) {
            char blockText[128];
            sprintf(blockText, "Looking at: %s", uiInfo.lookedAtBlockName);
            DrawTextInternal(blockText, 15.0f, 55.0f, 2.0f, Vector4(1, 1, 0.8f, 1), vertices, indices);
        } else {
            DrawTextInternal("Looking at: Nothing", 15.0f, 55.0f, 2.0f, Vector4(0.7f, 0.7f, 0.7f, 1), vertices, indices);
        }
    }

    if (vertices.empty()) {
        return;
    }

    UINT vbSize = static_cast<UINT>(vertices.size() * sizeof(UIVertex));
    UINT ibSize = static_cast<UINT>(indices.size() * sizeof(uint32_t));

    if (!m_uiVB || m_uiVB->GetDesc().Width < vbSize) {
        m_uiVB.Reset();
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vbSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                          IID_PPV_ARGS(m_uiVB.GetAddressOf()));
    }

    if (!m_uiIB || m_uiIB->GetDesc().Width < ibSize) {
        m_uiIB.Reset();
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = ibSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                          IID_PPV_ARGS(m_uiIB.GetAddressOf()));
    }

    void* mapped = nullptr;
    m_uiVB->Map(0, nullptr, &mapped);
    memcpy(mapped, vertices.data(), vbSize);
    m_uiVB->Unmap(0, nullptr);

    m_uiIB->Map(0, nullptr, &mapped);
    memcpy(mapped, indices.data(), ibSize);
    m_uiIB->Unmap(0, nullptr);

    m_commandList->SetPipelineState(m_uiPSO.Get());
    m_commandList->SetGraphicsRootSignature(m_uiRootSig.Get());

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = m_uiVB->GetGPUVirtualAddress();
    vbView.SizeInBytes = vbSize;
    vbView.StrideInBytes = sizeof(UIVertex);
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = m_uiIB->GetGPUVirtualAddress();
    ibView.SizeInBytes = ibSize;
    ibView.Format = DXGI_FORMAT_R32_UINT;

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &vbView);
    m_commandList->IASetIndexBuffer(&ibView);
    m_commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), 1, 0, 0, 0);
}

void DX12Renderer::UpdateRasterCB(const Camera& camera) {
    RasterCB cb = {};
    cb.world = Matrix4x4::Identity().Transpose();
    cb.view = camera.GetViewMatrix().Transpose();
    cb.projection = camera.GetProjectionMatrix().Transpose();

    Vector3 lightDir = Vector3(0.5f, -1.0f, 0.3f).normalized();
    cb.lightDir = Vector4(lightDir.x, lightDir.y, lightDir.z, 0.0f);

    void* mapped = nullptr;
    m_rasterCB->Map(0, nullptr, &mapped);
    memcpy(mapped, &cb, sizeof(cb));
    m_rasterCB->Unmap(0, nullptr);
}

bool DX12Renderer::RenderRasterization(World* world, const Camera& camera, const UIInfo& uiInfo,
                                       const Player* player, const std::vector<Mob*>& mobs) {
    if (!m_rasterReady || !world) {
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    world->GatherSolidMesh(vertices, indices);
    if (player && camera.GetMode() != CameraMode::FirstPerson) {
        AppendMeshTransformed(player->GetVertices(), player->GetIndices(), player->GetWorldMatrix(), vertices, indices);
    }
    for (const auto* mob : mobs) {
        if (mob) {
            AppendMeshTransformed(mob->GetVertices(), mob->GetIndices(), mob->GetWorldMatrix(), vertices, indices);
        }
    }
    if (vertices.empty() || indices.empty()) {
        return false;
    }

    UINT64 vbSize = vertices.size() * sizeof(Vertex);
    UINT64 ibSize = indices.size() * sizeof(uint32_t);

    if (!m_rasterVB || vbSize != m_rasterVBSize) {
        m_rasterVBSize = vbSize;
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vbSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(m_rasterVB.GetAddressOf())))) {
            return false;
        }
    }

    if (!m_rasterIB || ibSize != m_rasterIBSize) {
        m_rasterIBSize = ibSize;
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = ibSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(m_rasterIB.GetAddressOf())))) {
            return false;
        }
    }

    void* mapped = nullptr;
    m_rasterVB->Map(0, nullptr, &mapped);
    memcpy(mapped, vertices.data(), vbSize);
    m_rasterVB->Unmap(0, nullptr);

    m_rasterIB->Map(0, nullptr, &mapped);
    memcpy(mapped, indices.data(), ibSize);
    m_rasterIB->Unmap(0, nullptr);

    UpdateRasterCB(camera);

    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), m_rasterPSO.Get());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    float clearColor[4] = { 0.5f, 0.7f, 1.0f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MaxDepth = 1.0f;
    D3D12_RECT scissor = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissor);

    m_commandList->SetGraphicsRootSignature(m_rasterRootSig.Get());
    ID3D12DescriptorHeap* heaps[] = { m_rasterCbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, m_rasterCbvHeap->GetGPUDescriptorHandleForHeapStart());

    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = m_rasterVB->GetGPUVirtualAddress();
    vbView.SizeInBytes = static_cast<UINT>(vbSize);
    vbView.StrideInBytes = sizeof(Vertex);
    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = m_rasterIB->GetGPUVirtualAddress();
    ibView.SizeInBytes = static_cast<UINT>(ibSize);
    ibView.Format = DXGI_FORMAT_R32_UINT;

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &vbView);
    m_commandList->IASetIndexBuffer(&ibView);
    m_commandList->DrawIndexedInstanced(static_cast<UINT>(indices.size()), 1, 0, 0, 0);

    std::vector<Vertex> transVerts;
    std::vector<uint32_t> transIndices;
    world->GatherTransparentMesh(camera.GetPosition(), transVerts, transIndices);
    if (!transVerts.empty() && !transIndices.empty()) {
        UINT64 tvbSize = transVerts.size() * sizeof(Vertex);
        UINT64 tibSize = transIndices.size() * sizeof(uint32_t);

        if (!m_rasterVB || tvbSize > m_rasterVBSize) {
            m_rasterVBSize = tvbSize;
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = tvbSize;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(m_rasterVB.GetAddressOf()));
        }

        if (!m_rasterIB || tibSize > m_rasterIBSize) {
            m_rasterIBSize = tibSize;
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC desc = {};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = tibSize;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                              IID_PPV_ARGS(m_rasterIB.GetAddressOf()));
        }

        void* mapped = nullptr;
        m_rasterVB->Map(0, nullptr, &mapped);
        memcpy(mapped, transVerts.data(), tvbSize);
        m_rasterVB->Unmap(0, nullptr);

        m_rasterIB->Map(0, nullptr, &mapped);
        memcpy(mapped, transIndices.data(), tibSize);
        m_rasterIB->Unmap(0, nullptr);

        m_commandList->SetPipelineState(m_rasterTransparentPSO.Get());

        D3D12_VERTEX_BUFFER_VIEW tvbView = {};
        tvbView.BufferLocation = m_rasterVB->GetGPUVirtualAddress();
        tvbView.SizeInBytes = static_cast<UINT>(tvbSize);
        tvbView.StrideInBytes = sizeof(Vertex);
        D3D12_INDEX_BUFFER_VIEW tibView = {};
        tibView.BufferLocation = m_rasterIB->GetGPUVirtualAddress();
        tibView.SizeInBytes = static_cast<UINT>(tibSize);
        tibView.Format = DXGI_FORMAT_R32_UINT;

        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &tvbView);
        m_commandList->IASetIndexBuffer(&tibView);
        m_commandList->DrawIndexedInstanced(static_cast<UINT>(transIndices.size()), 1, 0, 0, 0);
        m_commandList->SetPipelineState(m_rasterPSO.Get());
    }

    RenderUIInternal(uiInfo);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);
    m_swapChain->Present(1, 0);
    MoveToNextFrame();
    return true;
}

bool DX12Renderer::InitializeRaytracing() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)))) {
        return false;
    }
    if (options5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        return false;
    }

    if (!CreateRaytracingPipeline()) return false;
    if (!CreateRaytracingResources()) return false;
    if (!CreateRaytracingOutput()) return false;
    if (!CreateShaderTable()) return false;
    return true;
}

bool DX12Renderer::CreateRaytracingPipeline() {
    ComPtr<ID3D12Device5> device5;
    if (FAILED(m_device->QueryInterface(IID_ID3D12Device5, reinterpret_cast<void**>(device5.GetAddressOf())))) {
        return false;
    }

    DxcLoader dxc;
    if (!dxc.Initialize()) {
        return false;
    }

    std::string source = ReadFileToString("shaders/Raytracing.hlsl");
    if (source.empty()) {
        return false;
    }

    std::vector<std::wstring> args;
    args.push_back(L"-T");
    args.push_back(L"lib_6_3");
    args.push_back(L"-Zi");
    args.push_back(L"-Qembed_debug");

    Microsoft::WRL::ComPtr<IDxcBlob> dxil;
    std::string errors;
    if (!dxc.CompileToDxil(L"Raytracing.hlsl", source, L"", L"lib_6_3", args, dxil, &errors)) {
        return false;
    }

    D3D12_DESCRIPTOR_RANGE1 ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 3;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER1 params[3] = {};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &ranges[1];

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[2].Descriptor.ShaderRegister = 0;
    params[2].Descriptor.RegisterSpace = 0;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = 3;
    rootSigDesc.Desc_1_1.pParameters = params;
    rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
    rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;
    rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> sigError;
    if (FAILED(D3D12SerializeVersionedRootSignature(&rootSigDesc, sigBlob.GetAddressOf(), sigError.GetAddressOf()))) {
        return false;
    }
    if (FAILED(m_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
                                             IID_PPV_ARGS(m_rtGlobalRootSig.GetAddressOf())))) {
        return false;
    }

    D3D12_EXPORT_DESC exports[] = {
        { L"RayGen", nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"Miss", nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"ClosestHit", nullptr, D3D12_EXPORT_FLAG_NONE }
    };

    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary.BytecodeLength = dxil->GetBufferSize();
    libDesc.DXILLibrary.pShaderBytecode = dxil->GetBufferPointer();
    libDesc.NumExports = 3;
    libDesc.pExports = exports;

    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"HitGroup";
    hitGroup.ClosestHitShaderImport = L"ClosestHit";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 4;
    shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2;

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_STATE_SUBOBJECT subobjects[5] = {};
    subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    subobjects[0].pDesc = &libDesc;
    subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[1].pDesc = &hitGroup;
    subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    subobjects[2].pDesc = &shaderConfig;
    subobjects[3].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    subobjects[3].pDesc = m_rtGlobalRootSig.GetAddressOf();
    subobjects[4].Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[4].pDesc = &pipelineConfig;

    D3D12_STATE_OBJECT_DESC stateDesc = {};
    stateDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateDesc.NumSubobjects = 5;
    stateDesc.pSubobjects = subobjects;

    void* stateObj = nullptr;
    if (FAILED(device5->CreateStateObject(&stateDesc, IID_ID3D12StateObject, &stateObj))) {
        return false;
    }
    m_rtStateObject.Attach(reinterpret_cast<ID3D12StateObject*>(stateObj));

    return true;
}

bool DX12Renderer::CreateRaytracingResources() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 4;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_srvUavHeap.GetAddressOf())))) {
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC cbDesc = {};
    cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbDesc.Width = sizeof(CameraCB);
    cbDesc.Height = 1;
    cbDesc.DepthOrArraySize = 1;
    cbDesc.MipLevels = 1;
    cbDesc.SampleDesc.Count = 1;
    cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(m_cameraCB.GetAddressOf())))) {
        return false;
    }

    return true;
}

bool DX12Renderer::CreateRaytracingOutput() {
    if (m_outputTexture && m_outputTexture->GetDesc().Width == m_width && m_outputTexture->GetDesc().Height == m_height) {
        return true;
    }

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = m_width;
    desc.Height = m_height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                 IID_PPV_ARGS(m_outputTexture.GetAddressOf())))) {
        return false;
    }
    m_outputInCopySource = false;

    return EnsureRaytracingDescriptors();
}

bool DX12Renderer::EnsureRaytracingDescriptors() {
    if (!m_srvUavHeap) return false;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();
    UINT increment = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // SRV 0: TLAS (created after build)
    handle.ptr += increment;

    // SRV 1: Vertex buffer
    handle.ptr += increment;

    // SRV 2: Index buffer
    handle.ptr += increment;

    // UAV 0: Output
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, handle);

    return true;
}

bool DX12Renderer::BuildAccelerationStructures(const std::vector<Vertex>& vertices,
                                               const std::vector<uint32_t>& indices) {
    if (vertices.empty() || indices.empty()) {
        return false;
    }

    UINT64 vertexBufferSize = vertices.size() * sizeof(Vertex);
    UINT64 indexBufferSize = indices.size() * sizeof(uint32_t);

    if (!m_vertexBuffer || !m_indexBuffer ||
        vertexBufferSize != m_vertexBufferSize || indexBufferSize != m_indexBufferSize) {
        m_vertexBufferSize = vertexBufferSize;
        m_indexBufferSize = indexBufferSize;

        D3D12_HEAP_PROPERTIES defaultHeap = {};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC vbDesc = {};
        vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        vbDesc.Width = vertexBufferSize;
        vbDesc.Height = 1;
        vbDesc.DepthOrArraySize = 1;
        vbDesc.MipLevels = 1;
        vbDesc.SampleDesc.Count = 1;
        vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())))) {
            return false;
        }

        D3D12_RESOURCE_DESC ibDesc = vbDesc;
        ibDesc.Width = indexBufferSize;
        if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &ibDesc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(m_indexBuffer.GetAddressOf())))) {
            return false;
        }
    }

    D3D12_HEAP_PROPERTIES uploadHeap2 = {};
    uploadHeap2.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = vertexBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> vbUpload;
    if (FAILED(m_device->CreateCommittedResource(&uploadHeap2, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(vbUpload.GetAddressOf())))) {
        return false;
    }

    void* mapped = nullptr;
    vbUpload->Map(0, nullptr, &mapped);
    memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
    vbUpload->Unmap(0, nullptr);

    uploadDesc.Width = indexBufferSize;
    ComPtr<ID3D12Resource> ibUpload;
    if (FAILED(m_device->CreateCommittedResource(&uploadHeap2, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(ibUpload.GetAddressOf())))) {
        return false;
    }

    ibUpload->Map(0, nullptr, &mapped);
    memcpy(mapped, indices.data(), indices.size() * sizeof(uint32_t));
    ibUpload->Unmap(0, nullptr);

    m_commandList->CopyBufferRegion(m_vertexBuffer.Get(), 0, vbUpload.Get(), 0, vertexBufferSize);
    m_commandList->CopyBufferRegion(m_indexBuffer.Get(), 0, ibUpload.Get(), 0, indexBufferSize);

    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = m_vertexBuffer.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[1] = barriers[0];
    barriers[1].Transition.pResource = m_indexBuffer.Get();
    m_commandList->ResourceBarrier(2, barriers);

    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    geomDesc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer->GetGPUVirtualAddress();
    geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    geomDesc.Triangles.VertexCount = static_cast<UINT>(vertices.size());
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.IndexBuffer = m_indexBuffer->GetGPUVirtualAddress();
    geomDesc.Triangles.IndexCount = static_cast<UINT>(indices.size());
    geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
    blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    blasInputs.NumDescs = 1;
    blasInputs.pGeometryDescs = &geomDesc;
    blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo = {};
    ComPtr<ID3D12Device5> device5;
    if (FAILED(m_device->QueryInterface(IID_ID3D12Device5, reinterpret_cast<void**>(device5.GetAddressOf())))) {
        return false;
    }
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC blasDesc = {};
    blasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    blasDesc.Width = blasInfo.ResultDataMaxSizeInBytes;
    blasDesc.Height = 1;
    blasDesc.DepthOrArraySize = 1;
    blasDesc.MipLevels = 1;
    blasDesc.SampleDesc.Count = 1;
    blasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    blasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &blasDesc,
                                                 D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                 IID_PPV_ARGS(m_blas.GetAddressOf())))) {
        return false;
    }

    D3D12_RESOURCE_DESC scratchDesc = blasDesc;
    scratchDesc.Width = blasInfo.ScratchDataSizeInBytes;
    ComPtr<ID3D12Resource> scratch;
    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &scratchDesc,
                                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                 IID_PPV_ARGS(scratch.GetAddressOf())))) {
        return false;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = blasInputs;
    buildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    buildDesc.DestAccelerationStructureData = m_blas->GetGPUVirtualAddress();

    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    if (FAILED(m_commandList->QueryInterface(IID_ID3D12GraphicsCommandList4, reinterpret_cast<void**>(cmdList4.GetAddressOf())))) {
        return false;
    }
    cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = m_blas.Get();
    m_commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
    tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlasInputs.NumDescs = 1;
    tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasInfo = {};
    device5->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);

    D3D12_RESOURCE_DESC tlasDesc = {};
    tlasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    tlasDesc.Width = tlasInfo.ResultDataMaxSizeInBytes;
    tlasDesc.Height = 1;
    tlasDesc.DepthOrArraySize = 1;
    tlasDesc.MipLevels = 1;
    tlasDesc.SampleDesc.Count = 1;
    tlasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    tlasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &tlasDesc,
                                                 D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr,
                                                 IID_PPV_ARGS(m_tlas.GetAddressOf())))) {
        return false;
    }

    D3D12_RESOURCE_DESC tlasScratchDesc = tlasDesc;
    tlasScratchDesc.Width = tlasInfo.ScratchDataSizeInBytes;
    ComPtr<ID3D12Resource> tlasScratch;
    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &tlasScratchDesc,
                                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                 IID_PPV_ARGS(tlasScratch.GetAddressOf())))) {
        return false;
    }

    D3D12_RESOURCE_DESC instanceDesc = {};
    instanceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    instanceDesc.Width = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    instanceDesc.Height = 1;
    instanceDesc.DepthOrArraySize = 1;
    instanceDesc.MipLevels = 1;
    instanceDesc.SampleDesc.Count = 1;
    instanceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> instanceBuffer;
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    if (FAILED(m_device->CreateCommittedResource(&uploadHeap2, D3D12_HEAP_FLAG_NONE, &instanceDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(instanceBuffer.GetAddressOf())))) {
        return false;
    }

    D3D12_RAYTRACING_INSTANCE_DESC* instance = nullptr;
    instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instance));
    memset(instance, 0, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
    instance->InstanceID = 0;
    instance->InstanceContributionToHitGroupIndex = 0;
    instance->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instance->AccelerationStructure = m_blas->GetGPUVirtualAddress();
    instance->InstanceMask = 0xFF;
    instance->Transform[0][0] = 1.0f;
    instance->Transform[1][1] = 1.0f;
    instance->Transform[2][2] = 1.0f;
    instanceBuffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild = {};
    tlasBuild.Inputs = tlasInputs;
    tlasBuild.Inputs.InstanceDescs = instanceBuffer->GetGPUVirtualAddress();
    tlasBuild.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
    tlasBuild.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();

    cmdList4->BuildRaytracingAccelerationStructure(&tlasBuild, 0, nullptr);
    uavBarrier.UAV.pResource = m_tlas.Get();
    m_commandList->ResourceBarrier(1, &uavBarrier);

    return true;
}

bool DX12Renderer::CreateShaderTable() {
    ComPtr<ID3D12StateObjectProperties> props;
    if (FAILED(m_rtStateObject->QueryInterface(IID_ID3D12StateObjectProperties, reinterpret_cast<void**>(props.GetAddressOf())))) {
        return false;
    }

    void* raygenId = props->GetShaderIdentifier(L"RayGen");
    void* missId = props->GetShaderIdentifier(L"Miss");
    void* hitId = props->GetShaderIdentifier(L"HitGroup");
    if (!raygenId || !missId || !hitId) {
        return false;
    }

    UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    UINT recordSize = (shaderIdSize + D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1) &
                      ~(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1);
    UINT tableSize = recordSize * 3;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = tableSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(m_shaderTable.GetAddressOf())))) {
        return false;
    }

    uint8_t* mapped = nullptr;
    m_shaderTable->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    memcpy(mapped, raygenId, shaderIdSize);
    memcpy(mapped + recordSize, missId, shaderIdSize);
    memcpy(mapped + recordSize * 2, hitId, shaderIdSize);
    m_shaderTable->Unmap(0, nullptr);

    return true;
}

void DX12Renderer::UpdateCameraCB(const Camera& camera) {
    CameraCB cb = {};
    Vector3 origin = camera.GetPosition();
    Vector3 forward = camera.GetForward();
    Vector3 right = camera.GetRight();
    Vector3 up = camera.GetUp();

    cb.origin[0] = origin.x;
    cb.origin[1] = origin.y;
    cb.origin[2] = origin.z;
    cb.forward[0] = forward.x;
    cb.forward[1] = forward.y;
    cb.forward[2] = forward.z;
    cb.right[0] = right.x;
    cb.right[1] = right.y;
    cb.right[2] = right.z;
    cb.up[0] = up.x;
    cb.up[1] = up.y;
    cb.up[2] = up.z;
    cb.viewport[0] = static_cast<float>(m_width);
    cb.viewport[1] = static_cast<float>(m_height);

    void* mapped = nullptr;
    m_cameraCB->Map(0, nullptr, &mapped);
    memcpy(mapped, &cb, sizeof(cb));
    m_cameraCB->Unmap(0, nullptr);
}

bool DX12Renderer::RenderRaytracing(World* world, const Camera& camera, const UIInfo& uiInfo) {
    if (!m_raytracingReady || !world) {
        return false;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    world->GatherSolidMesh(vertices, indices);
    if (vertices.empty() || indices.empty()) {
        return false;
    }

    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    if (!CreateRaytracingOutput()) {
        return false;
    }

    if (!BuildAccelerationStructures(vertices, indices)) {
        return false;
    }

    UpdateCameraCB(camera);

    if (!EnsureRaytracingDescriptors()) {
        return false;
    }

    UINT descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // SRV 0: TLAS
    D3D12_SHADER_RESOURCE_VIEW_DESC_RTX tlasSrv = {};
    tlasSrv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    tlasSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    tlasSrv.RaytracingAccelerationStructure.Location = m_tlas->GetGPUVirtualAddress();
    m_device->CreateShaderResourceView(nullptr,
                                       reinterpret_cast<const D3D12_SHADER_RESOURCE_VIEW_DESC*>(&tlasSrv),
                                       cpuHandle);

    // SRV 1: Vertex buffer (structured)
    cpuHandle.ptr += descriptorSize;
    D3D12_SHADER_RESOURCE_VIEW_DESC vbSrv = {};
    vbSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    vbSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    vbSrv.Buffer.FirstElement = 0;
    vbSrv.Buffer.NumElements = static_cast<UINT>(vertices.size());
    vbSrv.Buffer.StructureByteStride = sizeof(Vertex);
    vbSrv.Format = DXGI_FORMAT_UNKNOWN;
    m_device->CreateShaderResourceView(m_vertexBuffer.Get(), &vbSrv, cpuHandle);

    // SRV 2: Index buffer (raw)
    cpuHandle.ptr += descriptorSize;
    D3D12_SHADER_RESOURCE_VIEW_DESC ibSrv = {};
    ibSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    ibSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    ibSrv.Buffer.FirstElement = 0;
    ibSrv.Buffer.NumElements = static_cast<UINT>(indices.size());
    ibSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    ibSrv.Format = DXGI_FORMAT_R32_TYPELESS;
    m_device->CreateShaderResourceView(m_indexBuffer.Get(), &ibSrv, cpuHandle);

    // UAV 0: output
    cpuHandle.ptr += descriptorSize;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, cpuHandle);

    ComPtr<ID3D12GraphicsCommandList4> cmdList4;
    if (FAILED(m_commandList->QueryInterface(IID_ID3D12GraphicsCommandList4, reinterpret_cast<void**>(cmdList4.GetAddressOf())))) {
        return false;
    }

    if (m_outputInCopySource) {
        D3D12_RESOURCE_BARRIER toUav = {};
        toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toUav.Transition.pResource = m_outputTexture.Get();
        toUav.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &toUav);
        m_outputInCopySource = false;
    }

    cmdList4->SetPipelineState1(m_rtStateObject.Get());
    m_commandList->SetComputeRootSignature(m_rtGlobalRootSig.Get());

    ID3D12DescriptorHeap* heaps[] = { m_srvUavHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();
    m_commandList->SetComputeRootDescriptorTable(0, gpuHandle);
    gpuHandle.ptr += static_cast<UINT64>(descriptorSize) * 3;
    m_commandList->SetComputeRootDescriptorTable(1, gpuHandle);
    m_commandList->SetComputeRootConstantBufferView(2, m_cameraCB->GetGPUVirtualAddress());

    D3D12_DISPATCH_RAYS_DESC dispatch = {};
    UINT shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    UINT recordSize = (shaderIdSize + D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1) &
                      ~(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT - 1);

    D3D12_GPU_VIRTUAL_ADDRESS sbt = m_shaderTable->GetGPUVirtualAddress();
    dispatch.RayGenerationShaderRecord.StartAddress = sbt;
    dispatch.RayGenerationShaderRecord.SizeInBytes = recordSize;
    dispatch.MissShaderTable.StartAddress = sbt + recordSize;
    dispatch.MissShaderTable.SizeInBytes = recordSize;
    dispatch.MissShaderTable.StrideInBytes = recordSize;
    dispatch.HitGroupTable.StartAddress = sbt + recordSize * 2;
    dispatch.HitGroupTable.SizeInBytes = recordSize;
    dispatch.HitGroupTable.StrideInBytes = recordSize;
    dispatch.Width = m_width;
    dispatch.Height = m_height;
    dispatch.Depth = 1;

    cmdList4->DispatchRays(&dispatch);

    D3D12_RESOURCE_BARRIER outputBarrier = {};
    outputBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    outputBarrier.Transition.pResource = m_outputTexture.Get();
    outputBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    outputBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    outputBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &outputBarrier);
    m_outputInCopySource = true;

    D3D12_RESOURCE_BARRIER backBarrier = {};
    backBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    backBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    backBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    backBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    backBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &backBarrier);

    m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(), m_outputTexture.Get());

    D3D12_RESOURCE_BARRIER toRt = {};
    toRt.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRt.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    toRt.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toRt.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRt.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &toRt);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    RenderUIInternal(uiInfo);

    D3D12_RESOURCE_BARRIER presentBarrier = {};
    presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    presentBarrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &presentBarrier);

    m_commandList->Close();
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0);
    MoveToNextFrame();
    return true;
}

void DX12Renderer::WaitForGpu() {
    if (!m_commandQueue || !m_fence) return;
    const UINT64 fenceValue = m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fenceValue);
    m_fenceValue++;

    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Renderer::MoveToNextFrame() {
    const UINT64 fenceValue = m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fenceValue);
    m_fenceValue++;

    if (m_fence->GetCompletedValue() < fenceValue) {
        m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

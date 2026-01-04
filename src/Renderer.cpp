#include "Renderer.h"
#include "Window.h"
#include "World.h"
#include "Player.h"
#include "Mob.h"
#include "BlockDatabase.h"
#include "Chunk.h"
#include <d3dcompiler.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

Renderer::Renderer()
    : m_renderMode(RenderMode::Rasterization)
    , m_width(0)
    , m_height(0)
    , m_shadowMapSize(2048) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(Window* window) {
    m_width = window->GetWidth();
    m_height = window->GetHeight();

    if (!CreateDeviceAndSwapChain(window)) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateDepthStencil(m_width, m_height)) return false;
    if (!CreateShadowMap()) return false;
    if (!CompileShaders()) return false;
    if (!CreateConstantBuffers()) return false;
    if (!CreateSamplerStates()) return false;
    if (!CreateRasterizerStates()) return false;
    if (!CreateUIResources()) return false;

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    return true;
}

void Renderer::Shutdown() {
    if (m_context) {
        m_context->ClearState();
    }
}

bool Renderer::CreateDeviceAndSwapChain(Window* window) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = window->GetHandle();
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevel;
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_context
    );

    return SUCCEEDED(hr);
}

bool Renderer::CreateRenderTargets() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr)) return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_backBufferRTV);
    return SUCCEEDED(hr);
}

bool Renderer::CreateDepthStencil(int width, int height) {
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_device->CreateTexture2D(&depthDesc, nullptr, &m_depthStencilTexture);
    if (FAILED(hr)) return false;

    hr = m_device->CreateDepthStencilView(m_depthStencilTexture.Get(), nullptr, &m_depthStencilView);
    return SUCCEEDED(hr);
}

bool Renderer::CreateShadowMap() {
    D3D11_TEXTURE2D_DESC shadowDesc = {};
    shadowDesc.Width = m_shadowMapSize;
    shadowDesc.Height = m_shadowMapSize;
    shadowDesc.MipLevels = 1;
    shadowDesc.ArraySize = 1;
    shadowDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    shadowDesc.SampleDesc.Count = 1;
    shadowDesc.SampleDesc.Quality = 0;
    shadowDesc.Usage = D3D11_USAGE_DEFAULT;
    shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_device->CreateTexture2D(&shadowDesc, nullptr, &m_shadowMap);
    if (FAILED(hr)) return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = m_device->CreateDepthStencilView(m_shadowMap.Get(), &dsvDesc, &m_shadowMapDSV);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_device->CreateShaderResourceView(m_shadowMap.Get(), &srvDesc, &m_shadowMapSRV);
    return SUCCEEDED(hr);
}

bool Renderer::CompileShaderFromFile(const std::string& filename, const std::string& entryPoint,
                                     const std::string& profile, ID3DBlob** blob) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(
        source.c_str(),
        source.length(),
        filename.c_str(),
        nullptr,
        nullptr,
        entryPoint.c_str(),
        profile.c_str(),
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        blob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    return true;
}

bool Renderer::CompileShaders() {
    ComPtr<ID3DBlob> vsBlob, psBlob;

    // Block shaders
    if (!CompileShaderFromFile("shaders/BlockVertex.hlsl", "main", "vs_5_0", &vsBlob)) return false;
    if (!CompileShaderFromFile("shaders/BlockPixel.hlsl", "main", "ps_5_0", &psBlob)) return false;

    HRESULT hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_blockVS);
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_blockPS);
    if (FAILED(hr)) return false;

    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = m_device->CreateInputLayout(layout, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_inputLayout);
    if (FAILED(hr)) return false;

    // Shadow shaders
    if (!CompileShaderFromFile("shaders/ShadowVertex.hlsl", "main", "vs_5_0", &vsBlob)) return false;
    if (!CompileShaderFromFile("shaders/ShadowPixel.hlsl", "main", "ps_5_0", &psBlob)) return false;

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_shadowVS);
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_shadowPS);
    if (FAILED(hr)) return false;

    // UI shaders
    if (!CompileShaderFromFile("shaders/UIVertex.hlsl", "main", "vs_5_0", &vsBlob)) return false;
    if (!CompileShaderFromFile("shaders/UIPixel.hlsl", "main", "ps_5_0", &psBlob)) return false;

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_uiVS);
    if (FAILED(hr)) return false;

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_uiPS);
    if (FAILED(hr)) return false;

    // UI input layout (position + color)
    D3D11_INPUT_ELEMENT_DESC uiLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = m_device->CreateInputLayout(uiLayout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_uiInputLayout);
    return SUCCEEDED(hr);
}

bool Renderer::CreateConstantBuffers() {
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) return false;

    cbDesc.ByteWidth = 64; // For UI orthographic matrix
    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_uiConstantBuffer);
    return SUCCEEDED(hr);
}

bool Renderer::CreateSamplerStates() {
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = m_device->CreateSamplerState(&samplerDesc, &m_linearSampler);
    if (FAILED(hr)) return false;

    // PCF shadow sampler
    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_shadowSampler);
    return SUCCEEDED(hr);
}

bool Renderer::CreateRasterizerStates() {
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_BACK;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthBias = 0;
    rastDesc.DepthBiasClamp = 0.0f;
    rastDesc.SlopeScaledDepthBias = 0.0f;
    rastDesc.DepthClipEnable = TRUE;
    rastDesc.ScissorEnable = FALSE;
    rastDesc.MultisampleEnable = FALSE;
    rastDesc.AntialiasedLineEnable = FALSE;

    HRESULT hr = m_device->CreateRasterizerState(&rastDesc, &m_solidRasterizer);
    if (FAILED(hr)) return false;

    rastDesc.FillMode = D3D11_FILL_WIREFRAME;
    hr = m_device->CreateRasterizerState(&rastDesc, &m_wireframeRasterizer);
    return SUCCEEDED(hr);
}

bool Renderer::CreateUIResources() {
    // UI vertex and index buffers will be created dynamically
    return true;
}

void Renderer::BeginFrame() {
    float clearColor[4] = { 0.5f, 0.7f, 1.0f, 1.0f }; // Sky blue
    m_context->ClearRenderTargetView(m_backBufferRTV.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Renderer::EndFrame() {
    m_swapChain->Present(1, 0);
}

void Renderer::UpdateConstantBuffer(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj) {
    ConstantBuffer cb;
    cb.world = world.Transpose();
    cb.view = view.Transpose();
    cb.projection = proj.Transpose();

    // Simple directional light from above
    cb.lightDir = Vector4(0.5f, -1.0f, 0.3f, 0.0f);
    Vector3 lightDir(cb.lightDir.x, cb.lightDir.y, cb.lightDir.z);
    lightDir = lightDir.normalized();
    cb.lightDir = Vector4(lightDir.x, lightDir.y, lightDir.z, 0.0f);

    // Light view projection for shadows
    Vector3 lightPos(-100, 200, -100);
    Vector3 lightTarget(0, 60, 0);
    Matrix4x4 lightView = Matrix4x4::LookAtLH(lightPos, lightTarget, Vector3(0, 1, 0));
    Matrix4x4 lightProj = Matrix4x4::PerspectiveFovLH(1.57f, 1.0f, 1.0f, 500.0f);
    cb.lightViewProj = (lightView * lightProj).Transpose();

    D3D11_MAPPED_SUBRESOURCE mapped;
    m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cb, sizeof(ConstantBuffer));
    m_context->Unmap(m_constantBuffer.Get(), 0);
}

void Renderer::RenderShadowPass(World* world) {
    // Shadow pass rendering would go here
    // For now, we'll skip it to keep the implementation simpler
}

void Renderer::RenderWorld(World* world, Camera& camera) {
    if (m_renderMode == RenderMode::Raytracing) {
        // Stub for future DXR implementation
        // TODO: Implement raytracing pass
        return;
    }

    // Set render targets
    m_context->OMSetRenderTargets(1, m_backBufferRTV.GetAddressOf(), m_depthStencilView.Get());

    // Set rasterizer state
    m_context->RSSetState(m_solidRasterizer.Get());

    // Set shaders
    m_context->VSSetShader(m_blockVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_blockPS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_inputLayout.Get());

    // Set samplers
    m_context->PSSetSamplers(0, 1, m_linearSampler.GetAddressOf());
    m_context->PSSetSamplers(1, 1, m_shadowSampler.GetAddressOf());

    // Update and set constant buffer
    Matrix4x4 worldMatrix = Matrix4x4::Identity();
    Matrix4x4 viewMatrix = camera.GetViewMatrix();
    Matrix4x4 projMatrix = camera.GetProjectionMatrix();

    UpdateConstantBuffer(worldMatrix, viewMatrix, projMatrix);

    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    // Render world
    world->Render(m_context.Get());
}

void Renderer::RenderMob(Mob* mob, Camera& camera) {
    if (!mob) return;

    Matrix4x4 worldMatrix = mob->GetWorldMatrix();
    Matrix4x4 viewMatrix = camera.GetViewMatrix();
    Matrix4x4 projMatrix = camera.GetProjectionMatrix();

    UpdateConstantBuffer(worldMatrix, viewMatrix, projMatrix);

    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    mob->Render(m_context.Get());
}

void Renderer::RenderUI(Player* player) {
    // Simple UI rendering - draw inventory bar
    // This is a simplified implementation
    struct UIVertex {
        Vector2 position;
        Vector4 color;
    };

    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;

    // Draw inventory slots
    float slotSize = 50.0f;
    float spacing = 10.0f;
    float startX = m_width / 2.0f - (9 * slotSize + 8 * spacing) / 2.0f;
    float startY = m_height - slotSize - 20.0f;

    for (int i = 0; i < 9; i++) {
        float x = startX + i * (slotSize + spacing);
        float y = startY;

        Vector4 color = (i == player->GetSelectedSlot()) ? Vector4(1, 1, 0, 1) : Vector4(0.5f, 0.5f, 0.5f, 0.8f);

        // Convert to NDC
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

    if (vertices.empty()) return;

    // Create/update UI buffers
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(UIVertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    m_device->CreateBuffer(&vbDesc, &vbData, m_uiVertexBuffer.ReleaseAndGetAddressOf());

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    m_device->CreateBuffer(&ibDesc, &ibData, m_uiIndexBuffer.ReleaseAndGetAddressOf());

    // Render UI
    m_context->VSSetShader(m_uiVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_uiPS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_uiInputLayout.Get());

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_uiVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_uiIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);
}

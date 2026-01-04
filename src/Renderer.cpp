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

// Simple 5x7 bitmap font data for ASCII 32-127
// Each character is 5 pixels wide, 7 pixels tall
static const uint8_t g_fontData[96][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x04,0x04,0x04,0x04,0x00,0x04,0x00}, // '!'
    {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00}, // '#'
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04}, // '$'
    {0x18,0x19,0x02,0x04,0x08,0x13,0x03}, // '%'
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D}, // '&'
    {0x04,0x04,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, // '('
    {0x08,0x04,0x02,0x02,0x02,0x04,0x08}, // ')'
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00}, // '*'
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x04,0x04,0x08}, // ','
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x04,0x00}, // '.'
    {0x00,0x01,0x02,0x04,0x08,0x10,0x00}, // '/'
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, // '0'
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // '1'
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, // '2'
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, // '3'
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // '4'
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // '5'
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // '6'
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // '7'
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // '8'
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // '9'
    {0x00,0x04,0x00,0x00,0x04,0x00,0x00}, // ':'
    {0x00,0x04,0x00,0x00,0x04,0x04,0x08}, // ';'
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02}, // '<'
    {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00}, // '='
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08}, // '>'
    {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}, // '?'
    {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}, // '@'
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, // 'A'
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, // 'B'
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, // 'C'
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, // 'D'
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, // 'E'
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, // 'F'
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, // 'G'
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, // 'H'
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, // 'I'
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, // 'J'
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, // 'K'
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, // 'L'
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, // 'M'
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, // 'N'
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // 'O'
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, // 'P'
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, // 'Q'
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, // 'R'
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, // 'S'
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, // 'T'
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, // 'U'
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, // 'V'
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, // 'W'
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, // 'X'
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, // 'Y'
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, // 'Z'
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}, // '['
    {0x00,0x10,0x08,0x04,0x02,0x01,0x00}, // '\'
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}, // ']'
    {0x04,0x0A,0x11,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}, // '_'
    {0x08,0x04,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}, // 'a'
    {0x10,0x10,0x16,0x19,0x11,0x11,0x1E}, // 'b'
    {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E}, // 'c'
    {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F}, // 'd'
    {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}, // 'e'
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08}, // 'f'
    {0x00,0x0F,0x11,0x11,0x0F,0x01,0x0E}, // 'g'
    {0x10,0x10,0x16,0x19,0x11,0x11,0x11}, // 'h'
    {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}, // 'i'
    {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}, // 'j'
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12}, // 'k'
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}, // 'l'
    {0x00,0x00,0x1A,0x15,0x15,0x11,0x11}, // 'm'
    {0x00,0x00,0x16,0x19,0x11,0x11,0x11}, // 'n'
    {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}, // 'o'
    {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}, // 'p'
    {0x00,0x00,0x0D,0x13,0x0F,0x01,0x01}, // 'q'
    {0x00,0x00,0x16,0x19,0x10,0x10,0x10}, // 'r'
    {0x00,0x00,0x0E,0x10,0x0E,0x01,0x1E}, // 's'
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06}, // 't'
    {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}, // 'u'
    {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}, // 'v'
    {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}, // 'w'
    {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}, // 'x'
    {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}, // 'y'
    {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}, // 'z'
    {0x02,0x04,0x04,0x08,0x04,0x04,0x02}, // '{'
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04}, // '|'
    {0x08,0x04,0x04,0x02,0x04,0x04,0x08}, // '}'
    {0x00,0x00,0x08,0x15,0x02,0x00,0x00}, // '~'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // DEL
};

Renderer::Renderer()
    : m_renderMode(RenderMode::Rasterization)
    , m_width(0)
    , m_height(0)
    , m_shadowMapSize(2048)
    , m_debugHUDVisible(false) {
}

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(Window* window) {
    m_width = window->GetWidth();
    m_height = window->GetHeight();

    if (!CreateDeviceAndSwapChain(window)) {
        MessageBox(nullptr, "Failed to create D3D11 device and swap chain", "Error", MB_OK);
        return false;
    }
    if (!CreateRenderTargets()) {
        MessageBox(nullptr, "Failed to create render targets", "Error", MB_OK);
        return false;
    }
    if (!CreateDepthStencil(m_width, m_height)) {
        MessageBox(nullptr, "Failed to create depth stencil", "Error", MB_OK);
        return false;
    }
    if (!CreateShadowMap()) {
        MessageBox(nullptr, "Failed to create shadow map", "Error", MB_OK);
        return false;
    }
    if (!CompileShaders()) {
        MessageBox(nullptr, "Failed to compile shaders - check shader files in shaders/ directory", "Error", MB_OK);
        return false;
    }
    if (!CreateConstantBuffers()) {
        MessageBox(nullptr, "Failed to create constant buffers", "Error", MB_OK);
        return false;
    }
    if (!CreateSamplerStates()) {
        MessageBox(nullptr, "Failed to create sampler states", "Error", MB_OK);
        return false;
    }
    if (!CreateRasterizerStates()) {
        MessageBox(nullptr, "Failed to create rasterizer states", "Error", MB_OK);
        return false;
    }
    if (!CreateUIResources()) {
        MessageBox(nullptr, "Failed to create UI resources", "Error", MB_OK);
        return false;
    }

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
        // Try with "bin/" prefix
        std::ifstream file2("bin/" + filename);
        if (!file2.is_open()) {
            char msg[512];
            sprintf(msg, "Could not open shader file: %s", filename.c_str());
            MessageBox(nullptr, msg, "Shader Error", MB_OK);
            return false;
        }
        file = std::move(file2);
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
            char msg[1024];
            sprintf(msg, "Shader compilation failed for %s:\n%s",
                    filename.c_str(),
                    static_cast<char*>(errorBlob->GetBufferPointer()));
            MessageBox(nullptr, msg, "Shader Error", MB_OK);
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
    // Chunk and mob meshes are authored with CCW winding.
    rastDesc.FrontCounterClockwise = TRUE;
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
    if (FAILED(hr)) return false;

    // UI should not depend on the 3D pass' winding/culling state.
    D3D11_RASTERIZER_DESC uiRastDesc = rastDesc;
    uiRastDesc.FillMode = D3D11_FILL_SOLID;
    uiRastDesc.CullMode = D3D11_CULL_NONE;
    uiRastDesc.FrontCounterClockwise = FALSE;
    hr = m_device->CreateRasterizerState(&uiRastDesc, &m_uiRasterizer);
    if (FAILED(hr)) return false;

    // Create alpha blend state for UI
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_alphaBlendState);
    if (FAILED(hr)) return false;

    // Create depth stencil state for UI (disabled)
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = FALSE;

    hr = m_device->CreateDepthStencilState(&dsDesc, &m_depthDisabledState);
    if (FAILED(hr)) return false;

    // Create depth stencil state for transparent objects (read-only depth)
    dsDesc.DepthEnable = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = FALSE;

    hr = m_device->CreateDepthStencilState(&dsDesc, &m_depthReadOnlyState);
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

void Renderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width == m_width && height == m_height) {
        return;
    }

    m_width = width;
    m_height = height;

    if (m_context) {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    }

    m_backBufferRTV.Reset();
    m_depthStencilView.Reset();
    m_depthStencilTexture.Reset();

    m_swapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTargets();
    CreateDepthStencil(m_width, m_height);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
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
    // Matrices are built in column-vector form and transposed for the row-vector HLSL path.
    // To apply View then Projection in HLSL as mul(pos, LightViewProj), we need (Proj * View)^T here.
    cb.lightViewProj = (lightProj * lightView).Transpose();

    cb.cameraPos = Vector4(0, 0, 0, 1);

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

    // --- Transparent Pass ---
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_depthReadOnlyState.Get(), 0);

    world->RenderTransparent(m_context.Get(), camera.GetPosition());

    // Reset states
    m_context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(nullptr, 0);
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

void Renderer::RenderPlayer(Player* player, Camera& camera) {
    if (!player) return;
    if (camera.GetMode() == CameraMode::FirstPerson) return;

    Matrix4x4 worldMatrix = player->GetWorldMatrix();
    Matrix4x4 viewMatrix = camera.GetViewMatrix();
    Matrix4x4 projMatrix = camera.GetProjectionMatrix();

    UpdateConstantBuffer(worldMatrix, viewMatrix, projMatrix);

    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());

    player->Render(m_context.Get());
}

void Renderer::RenderUI(Player* player) {
    // Simple UI rendering - draw inventory bar
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

    // Render UI with alpha blending
    m_context->VSSetShader(m_uiVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_uiPS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_uiInputLayout.Get());
    m_context->RSSetState(m_uiRasterizer.Get());

    // Enable alpha blending and disable depth for UI
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_depthDisabledState.Get(), 0);

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_uiVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_uiIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);

    // Reset blend state and depth state
    m_context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(nullptr, 0);
}

bool Renderer::CreateFontResources() {
    // Font rendering uses simple quads, no texture needed
    return true;
}

void Renderer::DrawText(const std::string& text, float x, float y, float scale, const Vector4& color,
                        std::vector<UIVertex>& vertices, std::vector<uint32_t>& indices) {
    const float charWidth = 5.0f * scale;
    const float charHeight = 7.0f * scale;
    const float pixelScale = scale;
    const float spacing = 1.0f * scale;

    float cursorX = x;

    for (char c : text) {
        if (c < 32 || c > 127) c = '?';
        int charIndex = c - 32;

        // Draw each pixel of the character
        for (int row = 0; row < 7; row++) {
            uint8_t rowData = g_fontData[charIndex][row];
            for (int col = 0; col < 5; col++) {
                if (rowData & (1 << (4 - col))) {
                    float px = cursorX + col * pixelScale;
                    float py = y + row * pixelScale;

                    // Convert to NDC
                    float ndcX = (px / m_width) * 2.0f - 1.0f;
                    float ndcY = 1.0f - (py / m_height) * 2.0f;
                    float ndcW = (pixelScale / m_width) * 2.0f;
                    float ndcH = (pixelScale / m_height) * 2.0f;

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

void Renderer::RenderDebugHUD(const DebugInfo& debugInfo) {
    if (!m_debugHUDVisible) return;

    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;

    // Draw semi-transparent background
    float bgWidth = 300.0f;
    float bgHeight = 100.0f;
    float bgX = 10.0f;
    float bgY = 10.0f;

    float ndcX = (bgX / m_width) * 2.0f - 1.0f;
    float ndcY = 1.0f - (bgY / m_height) * 2.0f;
    float ndcW = (bgWidth / m_width) * 2.0f;
    float ndcH = (bgHeight / m_height) * 2.0f;

    Vector4 bgColor(0.0f, 0.0f, 0.0f, 0.5f);
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

    // Draw FPS text
    char fpsText[64];
    sprintf(fpsText, "FPS: %.1f", debugInfo.fps);
    DrawText(fpsText, 15.0f, 15.0f, 2.0f, Vector4(1.0f, 1.0f, 1.0f, 1.0f), vertices, indices);

    // Draw world stats
    char chunkText[64];
    sprintf(chunkText, "Chunks: %d", debugInfo.loadedChunkCount);
    DrawText(chunkText, 15.0f, 35.0f, 2.0f, Vector4(0.9f, 0.9f, 0.9f, 1.0f), vertices, indices);

    char indexText[128];
    sprintf(indexText, "Idx: %llu / %llu",
            static_cast<unsigned long long>(debugInfo.solidIndexCount),
            static_cast<unsigned long long>(debugInfo.transparentIndexCount));
    DrawText(indexText, 15.0f, 55.0f, 2.0f, Vector4(0.9f, 0.9f, 0.9f, 1.0f), vertices, indices);

    // Draw looked-at block info
    if (debugInfo.hasLookedAtBlock) {
        const char* blockName = BlockDatabase::GetProperties(debugInfo.lookedAtBlockType).name;
        char blockText[128];
        sprintf(blockText, "Looking at: %s", blockName);
        DrawText(blockText, 15.0f, 75.0f, 2.0f, Vector4(1.0f, 1.0f, 0.8f, 1.0f), vertices, indices);
    } else {
        DrawText("Looking at: Nothing", 15.0f, 75.0f, 2.0f, Vector4(0.7f, 0.7f, 0.7f, 1.0f), vertices, indices);
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

    ComPtr<ID3D11Buffer> debugVB;
    m_device->CreateBuffer(&vbDesc, &vbData, &debugVB);

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    ComPtr<ID3D11Buffer> debugIB;
    m_device->CreateBuffer(&ibDesc, &ibData, &debugIB);

    // Render debug HUD with alpha blending
    m_context->VSSetShader(m_uiVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_uiPS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_uiInputLayout.Get());
    m_context->RSSetState(m_uiRasterizer.Get());

    // Enable alpha blending and disable depth for UI
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(m_alphaBlendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_depthDisabledState.Get(), 0);

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, debugVB.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(debugIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_context->DrawIndexed(static_cast<UINT>(indices.size()), 0, 0);

    // Reset blend state and depth state
    m_context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(nullptr, 0);
}

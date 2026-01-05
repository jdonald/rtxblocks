#include "DxcLoader.h"
#include <guiddef.h>

using Microsoft::WRL::ComPtr;

namespace {
// {6245D6AF-66E0-48FD-80B4-4D271796748C}
static const GUID CLSID_DxcLibrary = {0x6245d6af, 0x66e0, 0x48fd, {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c}};
// {73E22D93-E6CE-47F3-B5BF-F0664F39C1B0}
static const GUID CLSID_DxcCompiler = {0x73e22d93, 0xe6ce, 0x47f3, {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0}};

// {E5204DC7-D18C-4C3C-BDFB-851673980FE7}
static const GUID IID_IDxcLibrary = {0xe5204dc7, 0xd18c, 0x4c3c, {0xbd, 0xfb, 0x85, 0x16, 0x73, 0x98, 0x0f, 0xe7}};
// {8CA3E215-F728-4CF3-8CDD-88AF917587A1}
static const GUID IID_IDxcCompiler = {0x8ca3e215, 0xf728, 0x4cf3, {0x8c, 0xdd, 0x88, 0xaf, 0x91, 0x75, 0x87, 0xa1}};
} // namespace

DxcLoader::DxcLoader()
    : m_dxcModule(nullptr)
    , m_createInstance(nullptr) {
}

DxcLoader::~DxcLoader() {
    if (m_dxcModule) {
        FreeLibrary(m_dxcModule);
        m_dxcModule = nullptr;
    }
}

bool DxcLoader::Initialize() {
    if (m_createInstance) {
        return true;
    }

    m_dxcModule = LoadLibraryW(L"dxcompiler.dll");
    if (!m_dxcModule) {
        return false;
    }

    m_createInstance = reinterpret_cast<DxcCreateInstanceProc>(
        GetProcAddress(m_dxcModule, "DxcCreateInstance"));
    if (!m_createInstance) {
        return false;
    }

    if (FAILED(m_createInstance(CLSID_DxcLibrary, IID_IDxcLibrary, reinterpret_cast<void**>(m_library.GetAddressOf())))) {
        return false;
    }

    if (FAILED(m_createInstance(CLSID_DxcCompiler, IID_IDxcCompiler, reinterpret_cast<void**>(m_compiler.GetAddressOf())))) {
        return false;
    }

    return true;
}

bool DxcLoader::CompileToDxil(const std::wstring& sourceName,
                              const std::string& sourceText,
                              const std::wstring& entryPoint,
                              const std::wstring& targetProfile,
                              const std::vector<std::wstring>& arguments,
                              ComPtr<IDxcBlob>& outBlob,
                              std::string* errorText) {
    if (!m_compiler || !m_library) {
        return false;
    }

    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = m_library->CreateBlobWithEncodingOnHeapCopy(
        sourceText.data(),
        static_cast<UINT32>(sourceText.size()),
        CP_UTF8,
        sourceBlob.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    std::vector<LPCWSTR> args;
    args.reserve(arguments.size());
    for (const auto& arg : arguments) {
        args.push_back(arg.c_str());
    }

    ComPtr<IDxcIncludeHandler> includeHandler;
    hr = m_library->CreateIncludeHandler(includeHandler.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    ComPtr<IDxcOperationResult> result;
    hr = m_compiler->Compile(
        sourceBlob.Get(),
        sourceName.c_str(),
        entryPoint.c_str(),
        targetProfile.c_str(),
        args.empty() ? nullptr : args.data(),
        static_cast<UINT32>(args.size()),
        nullptr,
        0,
        includeHandler.Get(),
        result.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    HRESULT status = S_OK;
    result->GetStatus(&status);
    if (FAILED(status)) {
        if (errorText) {
            ComPtr<IDxcBlobEncoding> errors;
            if (SUCCEEDED(result->GetErrorBuffer(errors.GetAddressOf())) && errors) {
                const char* text = static_cast<const char*>(errors->GetBufferPointer());
                *errorText = text ? std::string(text, errors->GetBufferSize()) : std::string();
            }
        }
        return false;
    }

    ComPtr<IDxcBlob> compiled;
    if (FAILED(result->GetResult(compiled.GetAddressOf()))) {
        return false;
    }

    outBlob = compiled;
    return true;
}

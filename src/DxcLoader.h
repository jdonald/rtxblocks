#pragma once
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <vector>

// Minimal DXC interfaces to avoid depending on dxcapi.h in MinGW.
struct IDxcBlob : public IUnknown {
    virtual LPVOID GetBufferPointer() = 0;
    virtual SIZE_T GetBufferSize() = 0;
};

struct IDxcBlobEncoding : public IDxcBlob {
    virtual HRESULT GetEncoding(BOOL* known, UINT32* codePage) = 0;
};

struct IDxcIncludeHandler : public IUnknown {
    virtual HRESULT LoadSource(LPCWSTR filename, IDxcBlob** includeSource) = 0;
};

struct IDxcOperationResult : public IUnknown {
    virtual HRESULT GetStatus(HRESULT* status) = 0;
    virtual HRESULT GetResult(IDxcBlob** result) = 0;
    virtual HRESULT GetErrorBuffer(IDxcBlobEncoding** errors) = 0;
};

struct IDxcLibrary : public IUnknown {
    virtual HRESULT SetMalloc(IMalloc* pMalloc) = 0;
    virtual HRESULT CreateBlobFromBlob(IDxcBlob* blob, UINT32 offset, UINT32 length, IDxcBlob** result) = 0;
    virtual HRESULT CreateBlobFromFile(LPCWSTR fileName, UINT32* codePage, IDxcBlobEncoding** blobEncoding) = 0;
    virtual HRESULT CreateBlobWithEncodingFromPinned(LPCVOID text, UINT32 size, UINT32 codePage, IDxcBlobEncoding** blobEncoding) = 0;
    virtual HRESULT CreateBlobWithEncodingOnHeapCopy(LPCVOID text, UINT32 size, UINT32 codePage, IDxcBlobEncoding** blobEncoding) = 0;
    virtual HRESULT CreateBlobWithEncodingOnMalloc(LPCVOID text, IMalloc* pMalloc, UINT32 size, UINT32 codePage, IDxcBlobEncoding** blobEncoding) = 0;
    virtual HRESULT CreateIncludeHandler(IDxcIncludeHandler** includeHandler) = 0;
};

struct IDxcCompiler : public IUnknown {
    virtual HRESULT Compile(
        IDxcBlob* source,
        LPCWSTR sourceName,
        LPCWSTR entryPoint,
        LPCWSTR targetProfile,
        LPCWSTR* arguments,
        UINT32 argCount,
        const struct DxcDefine* defines,
        UINT32 defineCount,
        IDxcIncludeHandler* includeHandler,
        IDxcOperationResult** result) = 0;
};

struct DxcDefine {
    LPCWSTR Name;
    LPCWSTR Value;
};

class DxcLoader {
public:
    DxcLoader();
    ~DxcLoader();

    bool Initialize();
    bool CompileToDxil(const std::wstring& sourceName,
                       const std::string& sourceText,
                       const std::wstring& entryPoint,
                       const std::wstring& targetProfile,
                       const std::vector<std::wstring>& arguments,
                       Microsoft::WRL::ComPtr<IDxcBlob>& outBlob,
                       std::string* errorText);

private:
    using DxcCreateInstanceProc = HRESULT (WINAPI*)(REFCLSID, REFIID, LPVOID*);

    HMODULE m_dxcModule;
    DxcCreateInstanceProc m_createInstance;
    Microsoft::WRL::ComPtr<IDxcLibrary> m_library;
    Microsoft::WRL::ComPtr<IDxcCompiler> m_compiler;
};

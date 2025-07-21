// ---------- Standard Library Includes ----------

#include <algorithm>
#include <array> // for std::size
#include <format>
#include <iostream>
#include <stacktrace>
#include <stdexcept>

// ---------- Windows ----------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <wrl/client.h> // for ComPtr
namespace wrl = Microsoft::WRL;

// ---------- DirectX Math ----------

#include <DirectXMath.h>
namespace dx = DirectX;

// ---------- D3D11 and DXGI ----------

#include <d3d11.h>
#include <dxgi1_3.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")

// ---------- ImGui ----------

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

// ---------- HLSL Constant Buffers ----------

#define matrix dx::XMFLOAT4X4
#define float3 dx::XMFLOAT3
#include <ConstantBuffers.hlsli>
#undef matrix
#undef float3

// ---------- Shader Bytecode ----------

#include <VS.h>
#include <PS.h>

// ---------- Constants ----------

constexpr const char* WIN32_WINDOW_CLASS_NAME{ "brdfs_window_class" };
constexpr const char* WIN32_WINDOW_TITLE{ "BRDFs" };

// ---------- Assertions ----------

#define Crash(msg) throw std::runtime_error{ std::format("[CRASH]: {}\n{}", msg, std::stacktrace::current()) };
#define Unreachable() Crash("unreachable code path")
#define Check(p) do { if (!(p)) Crash("Assertion failed: " #p); } while (false)
#define CheckHR(hr) Check(SUCCEEDED(hr))

// ---------- Global State ----------

static bool s_did_resize{};

// ---------- Window Procedure ----------

// forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT Win32WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wparam, lparam))
    {
        return 1;
    }
    else
    {
        LRESULT result{};

        switch (message)
        {
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;
        case WM_SIZE:
        {
            s_did_resize = true;
            result = DefWindowProcA(hwnd, message, wparam, lparam);
        } break;
        default:
        {
            result = DefWindowProcA(hwnd, message, wparam, lparam);
        } break;
        }

        return result;
    }
}

// ---------- Win32 Utilities ----------

static void RegisterWin32WindowClass()
{
    WNDCLASSEXA wdc{};
    wdc.cbSize = sizeof(wdc);
    wdc.style = CS_HREDRAW | CS_VREDRAW;
    wdc.lpfnWndProc = Win32WindowProc;
    //wdc.cbClsExtra = ;
    //wdc.cbWndExtra = ;
    wdc.hInstance = GetModuleHandleA(nullptr);
    wdc.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    wdc.hCursor = LoadCursorA(nullptr, IDC_ARROW);;
    //wdc.hbrBackground = ;
    //wdc.lpszMenuName = ;
    wdc.lpszClassName = WIN32_WINDOW_CLASS_NAME;
    wdc.hIconSm = LoadIconA(nullptr, IDI_APPLICATION);

    Check(RegisterClassExA(&wdc));
}

static HWND CreateWin32Window()
{
    DWORD style{ WS_OVERLAPPEDWINDOW | WS_VISIBLE };
    HWND window{ CreateWindowExA(0, WIN32_WINDOW_CLASS_NAME, WIN32_WINDOW_TITLE, style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr) };
    Check(window);
    return window;
}

// ---------- D3D11 and DXGI Utilities ----------

static void SetupDXGIInforQueue()
{
    #if defined(_DEBUG)
    wrl::ComPtr<IDXGIInfoQueue> queue{};
    CheckHR(DXGIGetDebugInterface1(0, IID_PPV_ARGS(queue.ReleaseAndGetAddressOf())));
    CheckHR(queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
    CheckHR(queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
    #endif
}

static wrl::ComPtr<ID3D11Device> CreateD3D11Device()
{
    UINT flags{};
    #if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif
    D3D_FEATURE_LEVEL required_lvl{ D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL supported_lvl{};

    wrl::ComPtr<ID3D11Device> d3d_dev{};
    CheckHR(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &required_lvl, 1, D3D11_SDK_VERSION, d3d_dev.ReleaseAndGetAddressOf(), &supported_lvl, nullptr
    ));
    Check(required_lvl == supported_lvl);

    return d3d_dev;
}

static void SetupD3D11InfoQueue([[maybe_unused]] ID3D11Device* d3d_dev)
{
    #if defined(_DEBUG)
    wrl::ComPtr<ID3D11InfoQueue> queue{};
    CheckHR(d3d_dev->QueryInterface(queue.ReleaseAndGetAddressOf()));
    CheckHR(queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true));
    CheckHR(queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true));
    #endif
}

static wrl::ComPtr<IDXGISwapChain1> CreateDXGISwapChain(ID3D11Device* d3d_dev, HWND hwnd)
{
    // get dxgi device from d3d device
    wrl::ComPtr<IDXGIDevice> dxgi_dev{};
    CheckHR(d3d_dev->QueryInterface(dxgi_dev.ReleaseAndGetAddressOf()));

    // get dxgi adapter from dxgi device
    wrl::ComPtr<IDXGIAdapter> dxgi_adapter{};
    CheckHR(dxgi_dev->GetAdapter(dxgi_adapter.ReleaseAndGetAddressOf()));

    // get dxgi factory from dxgi adapter
    wrl::ComPtr<IDXGIFactory2> dxgi_factory{};
    CheckHR(dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.ReleaseAndGetAddressOf())));

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = 0; // use window width
    desc.Height = 0; // use window height
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = false;
    desc.SampleDesc = { .Count = 1, .Quality = 0 };
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2; // double buffering
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = 0;

    wrl::ComPtr<IDXGISwapChain1> swap_chain{};
    CheckHR(dxgi_factory->CreateSwapChainForHwnd(d3d_dev, hwnd, &desc, nullptr, nullptr, swap_chain.ReleaseAndGetAddressOf()));

    // disble ALT+ENTER full screen switch shortcut
    CheckHR(dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    return swap_chain;
}

class Framebuffer
{
public:
    Framebuffer();
    Framebuffer(ID3D11Device* d3d_dev, IDXGISwapChain1* swap_chain);
    ~Framebuffer() = default;
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&&) noexcept = default;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer& operator=(Framebuffer&&) noexcept = default;
public:
    ID3D11Texture2D* BackBuffer() const noexcept { return m_back_buffer.Get(); }
    ID3D11RenderTargetView* BackBufferRTV() const noexcept { return m_back_buffer_rtv.Get(); }

private:
    wrl::ComPtr<ID3D11Texture2D> m_back_buffer;
    wrl::ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
};

Framebuffer::Framebuffer()
    : m_back_buffer{}
    , m_back_buffer_rtv{}
{
}
Framebuffer::Framebuffer(ID3D11Device* d3d_dev, IDXGISwapChain1* swap_chain)
    : m_back_buffer{}
    , m_back_buffer_rtv{}
{
    // get swap chain back buffer
    CheckHR(swap_chain->GetBuffer(0, IID_PPV_ARGS(m_back_buffer.ReleaseAndGetAddressOf())));

    // create swap chain back buffer rtv
    CheckHR(d3d_dev->CreateRenderTargetView(m_back_buffer.Get(), nullptr, m_back_buffer_rtv.ReleaseAndGetAddressOf()));
}

class Mesh
{
public:
    static Mesh Cube(ID3D11Device* d3d_dev);
public:
    Mesh(ID3D11Device* d3d_dev, UINT vertex_count, UINT vertex_size, const void* vertices, UINT index_count, UINT index_size, const void* indices);
    ~Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh& operator=(Mesh&&) noexcept = delete;
public:
    ID3D11Buffer* const* Vertices() const noexcept { return m_vertices.GetAddressOf(); }
    ID3D11Buffer* Indices() const noexcept { return m_indices.Get(); }
    UINT VertexCount() const noexcept { return m_vertex_count; }
    UINT IndexCount() const noexcept { return m_index_count; }
    const UINT* Stride() const noexcept { return &m_stride; }
    DXGI_FORMAT IndexFormat() const noexcept { return m_index_format; }
    const UINT* Offset() const noexcept { return &m_offset; }
private:
    wrl::ComPtr<ID3D11Buffer> m_vertices;
    wrl::ComPtr<ID3D11Buffer> m_indices;
    UINT m_vertex_count;
    UINT m_index_count;
    UINT m_stride;
    DXGI_FORMAT m_index_format;
    UINT m_offset;
};

Mesh Mesh::Cube(ID3D11Device* d3d_dev)
{
    struct Vertex
    {
        dx::XMFLOAT3 position;
    };

    Vertex vertices[]
    {
        // front face (Z+)
        { { -0.5f, -0.5f, +0.5f } },
        { { +0.5f, -0.5f, +0.5f } },
        { { +0.5f, +0.5f, +0.5f } },
        { { -0.5f, +0.5f, +0.5f } },

        // back face (Z-)
        { { +0.5f, -0.5f, -0.5f } },
        { { -0.5f, -0.5f, -0.5f } },
        { { -0.5f, +0.5f, -0.5f } },
        { { +0.5f, +0.5f, -0.5f } },

        // left face (X-)
        { { -0.5f, -0.5f, -0.5f } },
        { { -0.5f, -0.5f, +0.5f } },
        { { -0.5f, +0.5f, +0.5f } },
        { { -0.5f, +0.5f, -0.5f } },

        // right face (X+)
        { { +0.5f, -0.5f, +0.5f } },
        { { +0.5f, -0.5f, -0.5f } },
        { { +0.5f, +0.5f, -0.5f } },
        { { +0.5f, +0.5f, +0.5f } },

        // top face (Y+)
        { { -0.5f, +0.5f, +0.5f } },
        { { +0.5f, +0.5f, +0.5f } },
        { { +0.5f, +0.5f, -0.5f } },
        { { -0.5f, +0.5f, -0.5f } },

        // bottom face (Y-)
        { { -0.5f, -0.5f, -0.5f } },
        { { +0.5f, -0.5f, -0.5f } },
        { { +0.5f, -0.5f, +0.5f } },
        { { -0.5f, -0.5f, +0.5f } },
    };

    UINT indices[]
    {
        // front
        0, 1, 2,
        0, 2, 3,

        // back
        4, 5, 6,
        4, 6, 7,

        // left
        8, 9,10,
        8,10,11,

        // right
        12,13,14,
        12,14,15,

        // top 
        16,17,18,
        16,18,19,

        // bottom
        20,21,22,
        20,22,23
    };

    return { d3d_dev, std::size(vertices), sizeof(*vertices), vertices, std::size(indices), sizeof(*indices), indices };
}

Mesh::Mesh(ID3D11Device* d3d_dev, UINT vertex_count, UINT vertex_size, const void* vertices, UINT index_count, UINT index_size, const void* indices)
    : m_vertices{}
    , m_indices{}
    , m_vertex_count{ vertex_count }
    , m_index_count{ index_count }
    , m_stride{ vertex_size }
    , m_index_format{}
    , m_offset{}
{
    Check(vertex_count > 0);
    Check(index_count > 0);
    Check(vertex_size > 0);
    Check(index_size > 0 && (index_size == 2 || index_size == 4));

    // set index format based on index stride
    switch (index_size)
    {
    case 2: { m_index_format = DXGI_FORMAT_R16_UINT; } break;
    case 4: { m_index_format = DXGI_FORMAT_R32_UINT; } break;
    default: { Unreachable(); } break;
    }

    // upload vertices to the GPU
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = m_vertex_count * vertex_size;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = vertices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, &data, m_vertices.ReleaseAndGetAddressOf()));
    }

    // upload indices to the GPU
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = m_index_count * index_size;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = indices;
        data.SysMemPitch = 0;
        data.SysMemSlicePitch = 0;
        CheckHR(d3d_dev->CreateBuffer(&desc, &data, m_indices.ReleaseAndGetAddressOf()));
    }
}

class SubresourceMap
{
public:
    SubresourceMap(ID3D11DeviceContext* d3d_ctx, ID3D11Resource* res, UINT subres_idx, D3D11_MAP map_type, UINT map_flags);
    ~SubresourceMap();
    SubresourceMap(const SubresourceMap&) = delete;
    SubresourceMap(SubresourceMap&&) noexcept = delete;
    SubresourceMap& operator=(const SubresourceMap&) = delete;
    SubresourceMap& operator=(SubresourceMap&&) noexcept = delete;
public:
    void* Data() { return m_mapped_subres.pData; }
private:
    ID3D11DeviceContext* m_d3d_ctx;
    ID3D11Resource* m_res;
    UINT m_subres_idx;
    D3D11_MAPPED_SUBRESOURCE m_mapped_subres;
};

SubresourceMap::SubresourceMap(ID3D11DeviceContext* d3d_ctx, ID3D11Resource* res, UINT subres_idx, D3D11_MAP map_type, UINT map_flags)
    : m_d3d_ctx{ d3d_ctx }
    , m_res{ res }
    , m_subres_idx{ subres_idx }
    , m_mapped_subres{}
{
    CheckHR(m_d3d_ctx->Map(m_res, m_subres_idx, map_type, map_flags, &m_mapped_subres));
}
SubresourceMap::~SubresourceMap()
{
    m_d3d_ctx->Unmap(m_res, m_subres_idx);
}

// ---------- ImGui Utilities ----------

class ImGuiHandle
{
public:
    ImGuiHandle(HWND hwnd, ID3D11Device* d3d_dev, ID3D11DeviceContext* d3d_ctx);
    ~ImGuiHandle();
    ImGuiHandle(const ImGuiHandle&) = delete;
    ImGuiHandle(ImGuiHandle&) noexcept = delete;
    ImGuiHandle& operator=(const ImGuiHandle&) = delete;
    ImGuiHandle& operator=(ImGuiHandle&) noexcept = delete;
public:
    void BeginFrame() const noexcept;
    void EndFrame(ID3D11RenderTargetView* rtv) const noexcept;
private:
    ID3D11DeviceContext* m_d3d_ctx;
};

ImGuiHandle::ImGuiHandle(HWND hwnd, ID3D11Device* d3d_dev, ID3D11DeviceContext* d3d_ctx)
    : m_d3d_ctx{ d3d_ctx }
{
    // Make process DPI aware and obtain main monitor scale
    //ImGui_ImplWin32_EnableDpiAwareness();
    //float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    //ImGuiStyle& style = ImGui::GetStyle();
    //style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    //style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(d3d_dev, m_d3d_ctx);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);
}
ImGuiHandle::~ImGuiHandle()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
void ImGuiHandle::BeginFrame() const noexcept
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}
void ImGuiHandle::EndFrame(ID3D11RenderTargetView* rtv) const noexcept
{
    ImGui::Render();
    m_d3d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

namespace ImGuiEx
{
    bool DragFloat3(const char* label, DirectX::XMFLOAT3& v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
    {
        float buf[3]{ v.x, v.y, v.z };
        bool res{ ImGui::DragFloat3(label, buf, v_speed, v_min, v_max, format, flags) };
        v.x = buf[0];
        v.y = buf[1];
        v.z = buf[2];
        return res;
    }
    bool ColorEdit3(const char* label, DirectX::XMFLOAT3& col, ImGuiColorEditFlags flags = 0)
    {
        float buf[3]{ col.x, col.y, col.z };
        bool res{ ImGui::ColorEdit3(label, buf, flags) };
        col.x = buf[0];
        col.y = buf[1];
        col.z = buf[2];
        return res;
    }
}

// ---------- Entry Point ----------

static void Entry()
{
    // make process DPI aware
    Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));

    // register win32 window class
    RegisterWin32WindowClass();

    // create win32 window
    HWND window{ CreateWin32Window() };

    // setup dxgi info queue
    SetupDXGIInforQueue();

    // create d3d11 device
    wrl::ComPtr<ID3D11Device> d3d_dev{ CreateD3D11Device() };

    // setup d3d11 info queue
    SetupD3D11InfoQueue(d3d_dev.Get());

    // get d3d11 immediate device context
    wrl::ComPtr<ID3D11DeviceContext> d3d_ctx{};
    d3d_dev->GetImmediateContext(d3d_ctx.ReleaseAndGetAddressOf());

    // create swap chain
    wrl::ComPtr<IDXGISwapChain1> swap_chain{ CreateDXGISwapChain(d3d_dev.Get(), window) }; // TODO: it will be used

    // create framebuffer
    Framebuffer framebuffer{ d3d_dev.Get(), swap_chain.Get() };

    // initialize ImGui
    ImGuiHandle imgui_handle{ window, d3d_dev.Get(), d3d_ctx.Get() };

    // shaders
    wrl::ComPtr<ID3D11VertexShader> vs{};
    CheckHR(d3d_dev->CreateVertexShader(VS_bytes, sizeof(VS_bytes), nullptr, vs.ReleaseAndGetAddressOf()));
    wrl::ComPtr<ID3D11PixelShader> ps{};
    CheckHR(d3d_dev->CreatePixelShader(PS_bytes, sizeof(PS_bytes), nullptr, ps.ReleaseAndGetAddressOf()));

    // input layout
    wrl::ComPtr<ID3D11InputLayout> input_layout{};
    {
        D3D11_INPUT_ELEMENT_DESC desc[]
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        CheckHR(d3d_dev->CreateInputLayout(desc, std::size(desc), VS_bytes, sizeof(VS_bytes), input_layout.ReleaseAndGetAddressOf()));
    }

    // default rasterizer state
    wrl::ComPtr<ID3D11RasterizerState> rs_default{};
    {
        D3D11_RASTERIZER_DESC desc{};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_BACK;
        desc.FrontCounterClockwise = true;
        desc.DepthBias = 0;
        desc.DepthBiasClamp = 0.0f;
        desc.SlopeScaledDepthBias = 0.0f;
        desc.DepthClipEnable = true;
        desc.ScissorEnable = false;
        desc.MultisampleEnable = false;
        desc.AntialiasedLineEnable = false;
        CheckHR(d3d_dev->CreateRasterizerState(&desc, rs_default.ReleaseAndGetAddressOf()));
    }

    // scene constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_scene{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(SceneConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_scene.ReleaseAndGetAddressOf()));
    }

    // object constant buffer
    wrl::ComPtr<ID3D11Buffer> cb_object{};
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(ObjectConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        CheckHR(d3d_dev->CreateBuffer(&desc, nullptr, cb_object.ReleaseAndGetAddressOf()));
    }

    // cube mesh
    Mesh cube{ Mesh::Cube(d3d_dev.Get()) };

    // camera
    float camera_fov_deg{ 45.0f };
    dx::XMFLOAT3 camera_position{ 2.0f, 2.0f, -5.0f };
    dx::XMFLOAT3 camera_target{};
    float camera_near{ 0.1f };
    float camera_far{ 100.0f };

    // sphere
    dx::XMFLOAT3 sphere_position{};
    dx::XMFLOAT3 sphere_color{ 1.0f, 0.0f, 0.0f };

    // light
    dx::XMFLOAT3 light_position{ 2.0f, 1.0f, 2.0f };
    dx::XMFLOAT3 light_color{ 1.0f, 1.0f, 1.0f };

    // main application loop
    {
        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            else
            {
                // handle resize event
                if (s_did_resize)
                {
                    d3d_ctx->ClearState(); // clear state (some resources may be implicitly referenced by the context)
                    framebuffer = {}; // destroy framebuffer
                    CheckHR(swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0)); // resize swap chain
                    framebuffer = { d3d_dev.Get(), swap_chain.Get() }; // create new frame buffer

                    s_did_resize = false; // resize event handled
                }

                // fetch window size
                float window_w{};
                float window_h{};
                {
                    RECT rect{};
                    Check(GetClientRect(window, &rect)); // the right and bottom members contain the width and height of the window
                    constexpr LONG MIN_WINDOW_DIMENSION{ 8 };
                    window_w = static_cast<float>(std::max(rect.right, MIN_WINDOW_DIMENSION)); // sanitize window size; having size 0 may yield to problems
                    window_h = static_cast<float>(std::max(rect.bottom, MIN_WINDOW_DIMENSION)); // sanitize window size; having size 0 may yield to problems
                }

                // render scene
                {
                    // configure viewport
                    D3D11_VIEWPORT viewport{};
                    viewport.TopLeftX = 0.0f;
                    viewport.TopLeftY = 0.0f;
                    viewport.Width = window_w;
                    viewport.Height = window_h;
                    viewport.MinDepth = 0.0f;
                    viewport.MaxDepth = 1.0f;

                    // clear back buffer
                    {
                        float clear_color[4]{ 0.2f, 0.3f, 0.3f, 1.0f };
                        d3d_ctx->ClearRenderTargetView(framebuffer.BackBufferRTV(), clear_color);
                    }

                    // prepare pipeline for drawing
                    {
                        ID3D11RenderTargetView* rtv{ framebuffer.BackBufferRTV() };
                        ID3D11Buffer* cbufs[]{ cb_scene.Get(), cb_object.Get() };

                        d3d_ctx->ClearState();

                        d3d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        d3d_ctx->IASetInputLayout(input_layout.Get());
                        d3d_ctx->VSSetShader(vs.Get(), nullptr, 0);
                        d3d_ctx->VSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->PSSetShader(ps.Get(), nullptr, 0);
                        d3d_ctx->PSSetConstantBuffers(0, std::size(cbufs), cbufs);
                        d3d_ctx->RSSetState(rs_default.Get());
                        d3d_ctx->RSSetViewports(1, &viewport);
                        d3d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
                    }

                    // upload scene constants
                    {
                        // compute view matrix
                        dx::XMMATRIX view{};
                        {
                            dx::XMVECTOR eye{ dx::XMLoadFloat3(&camera_position) };
                            dx::XMVECTOR target{ dx::XMLoadFloat3(&camera_target) };
                            dx::XMVECTOR up{ dx::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) };
                            view = dx::XMMatrixLookAtLH(eye, target, up);
                        }

                        // compute projection matrix
                        dx::XMMATRIX projection{};
                        {
                            float fov_rad{ dx::XMConvertToRadians(camera_fov_deg) };
                            float aspect{ window_w / window_h };
                            projection = dx::XMMatrixPerspectiveFovLH(fov_rad, aspect, camera_near, camera_far);
                        }

                        // upload
                        {
                            SubresourceMap map{ d3d_ctx.Get(), cb_scene.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                            auto constants{ static_cast<SceneConstants*>(map.Data()) };
                            dx::XMStoreFloat4x4(&constants->view, view);
                            dx::XMStoreFloat4x4(&constants->projection, projection);
                            constants->world_eye = camera_position;
                        }
                    }

                    // render sphere
                    {
                        // upload object constants
                        {
                            constexpr float RADIUS{ 0.5f }; // sphere radius (given by local box geometry)
                            constexpr float DIAMETER{ RADIUS * 2.0f }; // sphere diameter

                            // build model matrix
                            dx::XMMATRIX model{};
                            {
                                dx::XMVECTOR scaling{ dx::XMVectorSet(DIAMETER, DIAMETER, DIAMETER, 0.0f) };
                                dx::XMVECTOR origin{ dx::XMVectorZero() };
                                dx::XMVECTOR rotation{ dx::XMQuaternionIdentity() };
                                dx::XMVECTOR translation{ dx::XMLoadFloat3(&sphere_position) };
                                model = dx::XMMatrixAffineTransformation(scaling, origin, rotation, translation);
                            }

                            // upload
                            {
                                SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                dx::XMStoreFloat4x4(&constants->model, model);
                                constants->color = sphere_color;
                                constants->position = sphere_position;
                                constants->radius = RADIUS;
                            }
                        }

                        // set pipeline state
                        d3d_ctx->IASetIndexBuffer(cube.Indices(), cube.IndexFormat(), 0);
                        d3d_ctx->IASetVertexBuffers(0, 1, cube.Vertices(), cube.Stride(), cube.Offset());

                        // draw
                        d3d_ctx->DrawIndexed(cube.IndexCount(), 0, 0);
                    }

                    // render light
                    {
                        // upload object constants
                        {
                            constexpr float RADIUS{ 0.5f }; // light sphere radius (given by local box geometry)
                            constexpr float DIAMETER{ RADIUS * 2.0f }; // light sphere diameter

                            // build model matrix
                            dx::XMMATRIX model{};
                            {
                                dx::XMVECTOR scaling{ dx::XMVectorSet(DIAMETER, DIAMETER, DIAMETER, 0.0f) };
                                dx::XMVECTOR origin{ dx::XMVectorZero() };
                                dx::XMVECTOR rotation{ dx::XMQuaternionIdentity() };
                                dx::XMVECTOR translation{ dx::XMLoadFloat3(&light_position) };
                                model = dx::XMMatrixAffineTransformation(scaling, origin, rotation, translation);
                            }
                            
                            // upload
                            {
                                SubresourceMap map{ d3d_ctx.Get(), cb_object.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0 };
                                auto constants{ static_cast<ObjectConstants*>(map.Data()) };
                                dx::XMStoreFloat4x4(&constants->model, model);
                                constants->color = light_color;
                                constants->position = light_position;
                                constants->radius = RADIUS;
                            }
                        }

                        // set pipeline state
                        d3d_ctx->IASetIndexBuffer(cube.Indices(), cube.IndexFormat(), 0);
                        d3d_ctx->IASetVertexBuffers(0, 1, cube.Vertices(), cube.Stride(), cube.Offset());

                        // draw
                        d3d_ctx->DrawIndexed(cube.IndexCount(), 0, 0);
                    }
                }

                // render ImGui
                imgui_handle.BeginFrame();
                {
                    ImGui::Begin("BRDFs");
                    {
                        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGuiEx::DragFloat3("Position##Camera", camera_position, 0.01f);
                            ImGuiEx::DragFloat3("Target", camera_target, 0.01f);
                        }
                        if (ImGui::CollapsingHeader("Sphere", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGuiEx::DragFloat3("Position##Sphere", sphere_position, 0.01f);
                            ImGuiEx::ColorEdit3("Color##Sphere", sphere_color);
                        }
                        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            ImGuiEx::DragFloat3("Position##Light", light_position, 0.01f);
                            ImGuiEx::ColorEdit3("Color##Light", light_color);
                        }
                    }
                    ImGui::End();
                }
                imgui_handle.EndFrame(framebuffer.BackBufferRTV());

                // present
                {
                    CheckHR(swap_chain->Present(1, 0)); // present with vsync
                }
            }
        }
    }
}

// ---------- Main ----------

int main()
{
    try
    {
        Entry();
    }
    catch (const std::runtime_error& e)
    {
        std::cout << e.what() << "\n";
    }

    return 0;
}

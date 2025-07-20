// ---------- Standard Library Includes ----------

#include <format>
#include <iostream>
#include <stacktrace>
#include <stdexcept>

// ---------- Windows ----------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <wrl/client.h> // for ComPtr
namespace wrl = Microsoft::WRL;

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

// ---------- Constants ----------

constexpr const char* WIN32_WINDOW_CLASS_NAME{ "brdfs_window_class" };
constexpr const char* WIN32_WINDOW_TITLE{ "BRDFs" };

// ---------- Assertions ----------

#define Crash(msg) throw std::runtime_error{ std::format("[CRASH]: {}\n{}", msg, std::stacktrace::current()) };
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

                // TODO: update

                // render scene
                {
                    // clear back buffer
                    {
                        float clear_color[4]{ 0.2f, 0.3f, 0.3f, 1.0f };
                        d3d_ctx->ClearRenderTargetView(framebuffer.BackBufferRTV(), clear_color);
                    }
                }

                // render ImGui
                imgui_handle.BeginFrame();
                {
                    ImGui::ShowDemoWindow();
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

// ---------- Standard Library Includes ----------

#include <format>
#include <iostream>
#include <stacktrace>
#include <stdexcept>

// ---------- Windows ----------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// ---------- Constants ----------

constexpr const char* WIN32_WINDOW_CLASS_NAME{ "brdfs_window_class" };
constexpr const char* WIN32_WINDOW_TITLE{ "BRDFs" };

// ---------- Assertions ----------

#define Crash(msg) throw std::runtime_error{ std::format("[CRASH]: {}\n{}", msg, std::stacktrace::current()) };
#define Check(p) do { if (!(p)) Crash("Assertion failed: " #p); } while (false)

// ---------- Window Procedure ----------

static LRESULT Win32WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result{};

    switch (message)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    default:
    {
        result = DefWindowProcA(hwnd, message, wparam, lparam);
    } break;
    }

    return result;
}

// ---------- Win32 Utilities ----------

static void Win32RegisterWindowClass()
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

static HWND Win32CreateWindow()
{
    DWORD style{ WS_OVERLAPPEDWINDOW | WS_VISIBLE };
    HWND window{ CreateWindowExA(0, WIN32_WINDOW_CLASS_NAME, WIN32_WINDOW_TITLE, style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr) };
    Check(window);
    return window;
}

// ---------- Entry Point ----------

static void Entry()
{
    // make process DPI aware
    Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE));

    // register win32 window class
    Win32RegisterWindowClass();

    // create win32 window
    [[maybe_unused]] HWND window{ Win32CreateWindow() };

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
                // TODO: update
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

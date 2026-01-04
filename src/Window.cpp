#include "Window.h"
#include <cstring>

Window::Window(int width, int height, const std::string& title)
    : m_hwnd(nullptr)
    , m_width(width)
    , m_height(height)
    , m_title(title)
    , m_mouseCaptured(false)
    , m_lastMouseX(0)
    , m_lastMouseY(0)
    , m_firstMouse(true)
    , m_hasFocus(true)
    , m_isFullscreen(false)
    , m_windowedStyle(0) {
    std::memset(m_keys, 0, sizeof(m_keys));
    std::memset(m_keysPressed, 0, sizeof(m_keysPressed));
    std::memset(&m_windowedRect, 0, sizeof(m_windowedRect));
}

Window::~Window() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

bool Window::Create() {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "RTXBlocksWindowClass";

    if (!RegisterClassEx(&wc)) {
        return false;
    }

    RECT rect = { 0, 0, m_width, m_height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    m_hwnd = CreateWindowEx(
        0,
        "RTXBlocksWindowClass",
        m_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!m_hwnd) {
        return false;
    }

    return true;
}

void Window::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

bool Window::ProcessMessages() {
    std::memset(m_keysPressed, 0, sizeof(m_keysPressed));

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

bool Window::IsKeyDown(int vkCode) const {
    if (vkCode < 0 || vkCode >= 256) return false;
    return m_keys[vkCode];
}

bool Window::WasKeyPressed(int vkCode) const {
    if (vkCode < 0 || vkCode >= 256) return false;
    return m_keysPressed[vkCode];
}

void Window::GetMouseDelta(int& dx, int& dy) {
    if (!m_mouseCaptured || !m_hasFocus) {
        dx = 0;
        dy = 0;
        return;
    }

    POINT pt;
    GetCursorPos(&pt);

    if (m_firstMouse) {
        m_lastMouseX = pt.x;
        m_lastMouseY = pt.y;
        m_firstMouse = false;
    }

    dx = pt.x - m_lastMouseX;
    dy = pt.y - m_lastMouseY;

    m_lastMouseX = pt.x;
    m_lastMouseY = pt.y;

    // Recenter cursor
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    POINT center = { rect.right / 2, rect.bottom / 2 };
    ClientToScreen(m_hwnd, &center);
    SetCursorPos(center.x, center.y);
    m_lastMouseX = center.x;
    m_lastMouseY = center.y;
}

void Window::SetMouseCapture(bool capture) {
    m_mouseCaptured = capture;
    if (capture) {
        ShowCursor(FALSE);
        m_firstMouse = true;
    } else {
        ShowCursor(TRUE);
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam < 256) {
            if (!m_keys[wParam]) {
                m_keysPressed[wParam] = true;
            }
            m_keys[wParam] = true;
        }
        return 0;

    case WM_KEYUP:
        if (wParam < 256) {
            m_keys[wParam] = false;
        }
        return 0;

    case WM_SIZE:
        m_width = LOWORD(lParam);
        m_height = HIWORD(lParam);
        return 0;

    case WM_ACTIVATEAPP:
        m_hasFocus = (wParam == TRUE);
        if (m_hasFocus) {
            // Regained focus - restore cursor state
            if (m_mouseCaptured) {
                ShowCursor(FALSE);
                m_firstMouse = true;
            }
        } else {
            // Lost focus - show cursor
            if (m_mouseCaptured) {
                ShowCursor(TRUE);
            }
        }
        return 0;

    case WM_SETFOCUS:
        m_hasFocus = true;
        if (m_mouseCaptured) {
            ShowCursor(FALSE);
            m_firstMouse = true;
        }
        return 0;

    case WM_KILLFOCUS:
        m_hasFocus = false;
        if (m_mouseCaptured) {
            ShowCursor(TRUE);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (!m_mouseCaptured) {
            SetMouseCapture(true);
        }
        return 0;
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

void Window::ToggleFullscreen() {
    if (!m_isFullscreen) {
        // Save current window rect and style
        m_windowedStyle = GetWindowLong(m_hwnd, GWL_STYLE);
        GetWindowRect(m_hwnd, &m_windowedRect);

        // Get monitor info
        HMONITOR hMon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(hMon, &mi);

        // Set fullscreen style and position
        SetWindowLong(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(m_hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

        m_isFullscreen = true;
    } else {
        // Restore windowed mode
        SetWindowLong(m_hwnd, GWL_STYLE, m_windowedStyle);
        SetWindowPos(m_hwnd, HWND_NOTOPMOST,
                     m_windowedRect.left, m_windowedRect.top,
                     m_windowedRect.right - m_windowedRect.left,
                     m_windowedRect.bottom - m_windowedRect.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

        m_isFullscreen = false;
    }
}

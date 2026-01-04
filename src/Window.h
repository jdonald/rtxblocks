#pragma once
#include <windows.h>
#include <string>
#include <functional>

class Window {
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    bool Create();
    void Show();
    bool ProcessMessages();

    HWND GetHandle() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    bool IsKeyDown(int vkCode) const;
    bool WasKeyPressed(int vkCode) const;
    void GetMouseDelta(int& dx, int& dy);
    void SetMouseCapture(bool capture);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd;
    int m_width;
    int m_height;
    std::string m_title;
    bool m_keys[256];
    bool m_keysPressed[256];
    bool m_mouseCaptured;
    int m_lastMouseX;
    int m_lastMouseY;
    bool m_firstMouse;
};

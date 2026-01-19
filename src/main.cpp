/**
 * @file main.cpp
 * @brief Kinect Football - FIFA 2026 Soccer Simulator Kiosk
 *
 * Portrait-mode interactive soccer experience using Azure Kinect
 * body tracking for kick detection and gamified challenges.
 *
 * Display: Portrait touchscreen (1080x1920)
 * External: LED scoreboard via serial
 */

#include "gui/Application.h"
#include <windows.h>
#include <iostream>

// Forward declare ImGui Win32 handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    kinect::gui::Application* app = reinterpret_cast<kinect::gui::Application*>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (msg) {
        case WM_SIZE:
            if (app) {
                app->onResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_KEYDOWN:
            if (app) {
                app->onKeyDown(static_cast<int>(wParam));
            }
            // F12 = Kinect restart (from kinect-native)
            if (wParam == VK_F12) {
                if (app) {
                    app->onKinectRestart();
                }
            }
            // ESC = Exit (kiosk mode can disable this)
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    // Enable console for debug output
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    std::cout << "===========================================\n";
    std::cout << " Kinect Football - FIFA 2026 Simulator\n";
    std::cout << " Portrait Kiosk Mode (1080x1920)\n";
    std::cout << "===========================================\n\n";

    // Portrait mode dimensions
    const int WINDOW_WIDTH = 1080;
    const int WINDOW_HEIGHT = 1920;

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"KinectFootballClass";
    RegisterClassEx(&wc);

    // Create window (borderless for kiosk mode)
    DWORD style = WS_POPUP;  // Borderless for kiosk
    // For development, use: WS_OVERLAPPEDWINDOW

    HWND hWnd = CreateWindowEx(
        0,
        L"KinectFootballClass",
        L"Kinect Football - FIFA 2026",
        style,
        0, 0,  // Position at origin for kiosk
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr,
        hInstance, nullptr
    );

    if (!hWnd) {
        std::cerr << "Failed to create window\n";
        return 1;
    }

    // Create application
    kinect::gui::Application app;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));

    // Initialize application
    if (!app.initialize(hWnd, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        std::cerr << "Failed to initialize application\n";
        return 1;
    }

    // Show window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    std::cout << "Application started. Press F12 to restart Kinect, ESC to exit.\n\n";

    // Main message loop
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Render frame
            app.update();
            app.render();
        }
    }

    // Cleanup
    app.shutdown();

    return static_cast<int>(msg.wParam);
}

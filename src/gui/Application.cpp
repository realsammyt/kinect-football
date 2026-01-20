/**
 * @file Application.cpp
 * @brief Main application implementation for Kinect Football (Demo Mode)
 */

#include "Application.h"
#include "../include/UITheme.h"
#include "kiosk/KioskManager.h"
#include "kiosk/SessionManager.h"
#include <iostream>
#include <chrono>
#include <cmath>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

namespace kinect {
namespace gui {

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::initialize(HWND hWnd, int width, int height) {
    hWnd_ = hWnd;
    width_ = width;
    height_ = height;

    logInfo("Initializing application...");

    // Create D3D device
    if (!createD3DDevice()) {
        logError("Failed to create D3D device");
        return false;
    }

    // Create render target
    if (!createRenderTarget()) {
        logError("Failed to create render target");
        return false;
    }

    // Initialize ImGui
    if (!initImGui()) {
        logError("Failed to initialize ImGui");
        return false;
    }

    // Demo mode - no Kinect/player tracker needed

    running_ = true;
    gameState_ = GameState::Attract;
    stateStartTime_ = std::chrono::steady_clock::now();

    logInfo("Application initialized successfully (Demo Mode - No Kinect)");
    return true;
}

void Application::shutdown() {
    logInfo("Shutting down application...");

    running_ = false;

    // Cleanup ImGui
    cleanupImGui();

    // Cleanup D3D
    cleanupRenderTarget();
    cleanupD3DDevice();

    logInfo("Application shutdown complete");
}

void Application::update() {
    updateStateLogic();
}

void Application::render() {
    if (!d3dContext_ || !swapChain_ || !renderTarget_) {
        return;
    }

    // Clear background based on selected theme
    float clearColor[4];
    getBackgroundClearColor(selectedBackground_, clearColor);
    d3dContext_->ClearRenderTargetView(renderTarget_, clearColor);

    // Start ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Render based on game state
    switch (gameState_) {
        case GameState::Attract:
            renderAttractMode();
            break;
        case GameState::PlayerDetected:
            renderPlayerDetected();
            break;
        case GameState::SelectingOptions:
            renderOptionsSelect();
            break;
        case GameState::SelectingChallenge:
            renderChallengeSelect();
            break;
        case GameState::Countdown:
            renderCountdown();
            break;
        case GameState::Playing:
            renderGameplay();
            break;
        case GameState::Results:
            renderResults();
            break;
        case GameState::Celebration:
            renderCelebration();
            break;
        case GameState::Error:
            renderError();
            break;
    }

    // Render ImGui
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        d3dContext_->OMSetRenderTargets(1, &renderTarget_, nullptr);
        ImGui_ImplDX11_RenderDrawData(drawData);
    }

    // Present
    swapChain_->Present(1, 0);
}

void Application::onResize(int width, int height) {
    if (width == 0 || height == 0) return;

    width_ = width;
    height_ = height;

    cleanupRenderTarget();
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        logError("Failed to resize swap chain buffers");
        return;
    }
    createRenderTarget();
}

void Application::onKeyDown(int key) {
    // Handle key presses for testing
    switch (key) {
        case '1':
            transitionTo(GameState::Attract);
            break;
        case '2':
            transitionTo(GameState::PlayerDetected);
            break;
        case '3':
            transitionTo(GameState::SelectingChallenge);
            break;
        case '4':
            transitionTo(GameState::Playing);
            break;
        case '5':
            transitionTo(GameState::Results);
            break;
        case VK_SPACE:
            // Simulate player detection
            transitionTo(GameState::PlayerDetected);
            break;
    }
}

void Application::onKinectRestart() {
    logInfo("Kinect restart requested (Demo Mode - No Action)");
}

// D3D setup
bool Application::createD3DDevice() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd_;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &swapChain_, &d3dDevice_, &featureLevel, &d3dContext_
    );

    return SUCCEEDED(hr);
}

void Application::cleanupD3DDevice() {
    if (d3dContext_) { d3dContext_->Release(); d3dContext_ = nullptr; }
    if (d3dDevice_) { d3dDevice_->Release(); d3dDevice_ = nullptr; }
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
}

bool Application::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    HRESULT hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr) || !backBuffer) {
        logError("Failed to get back buffer from swap chain");
        return false;
    }

    hr = d3dDevice_->CreateRenderTargetView(backBuffer, nullptr, &renderTarget_);
    backBuffer->Release();

    if (FAILED(hr)) {
        logError("Failed to create render target view");
        return false;
    }

    return renderTarget_ != nullptr;
}

void Application::cleanupRenderTarget() {
    if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
}

// ImGui setup
bool Application::initImGui() {
    IMGUI_CHECKVERSION();
    imguiContext_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style for kiosk
    ImGui::StyleColorsDark();

    // FIFA 2026 Theme
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = kinect::theme::layout::CORNER_RADIUS;
    style.FramePadding = ImVec2(24, 16);
    style.FrameBorderSize = 2.0f;
    style.ItemSpacing = ImVec2(16, 12);
    style.GrabMinSize = kinect::theme::layout::MIN_TOUCH_TARGET;
    style.GrabRounding = 8.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.04f, 0.09f, 0.16f, 0.95f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.00f, 0.83f, 0.67f, 0.40f);
    colors[ImGuiCol_Button] = ImVec4(0.00f, 0.83f, 0.67f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.93f, 0.77f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.73f, 0.57f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.00f, 0.83f, 0.67f, 0.30f);

    // Scale for portrait display
    float scale = static_cast<float>(width_) / 1080.0f;
    style.ScaleAllSizes(scale);
    io.FontGlobalScale = scale;

    ImGui_ImplWin32_Init(hWnd_);
    ImGui_ImplDX11_Init(d3dDevice_, d3dContext_);

    return true;
}

void Application::cleanupImGui() {
    if (imguiContext_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imguiContext_);
        imguiContext_ = nullptr;
    }
}

// Rendering methods
void Application::renderAttractMode() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Attract", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;

    // Animated glow effect behind title
    float time = static_cast<float>(ImGui::GetTime());
    float pulse = 0.5f + 0.5f * sinf(time * 2.0f);
    float glowRadius = 150.0f + pulse * 50.0f;
    ImU32 glowColor = IM_COL32(0, 212, 171, static_cast<int>(40 * pulse));
    drawList->AddCircleFilled(ImVec2(centerX, height_ * 0.25f), glowRadius, glowColor);

    // Title
    const char* title = "KINECT FOOTBALL";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2(centerX - titleSize.x / 2, height_ * 0.25f),
        kinect::theme::colors::TEAL, title);

    // Subtitle
    const char* subtitle = "FIFA 2026 SIMULATOR";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    drawList->AddText(ImVec2(centerX - subSize.x / 2, height_ * 0.32f),
        kinect::theme::colors::GOLD, subtitle);

    // Instructions
    const char* instructions = "Press SPACE or step in front of camera";
    ImVec2 instrSize = ImGui::CalcTextSize(instructions);
    drawList->AddText(ImVec2(centerX - instrSize.x / 2, height_ * 0.6f),
        IM_COL32(200, 200, 200, 255), instructions);

    // Pulsing indicator
    float indicatorPulse = 0.5f + 0.5f * sinf(time * 3.0f);
    ImU32 pulseColor = IM_COL32(0, static_cast<int>(255 * indicatorPulse), 0, 255);
    drawList->AddCircleFilled(ImVec2(centerX, height_ * 0.75f), 20.0f, pulseColor);

    // Demo mode indicator
    drawList->AddText(ImVec2(10, height_ - 30),
        IM_COL32(100, 100, 100, 255), "DEMO MODE - Press 1-5 to change states");

    ImGui::End();
}

void Application::renderPlayerDetected() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Detected", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;

    const char* text = "PLAYER DETECTED!";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(ImVec2(centerX - textSize.x / 2, height_ * 0.4f),
        IM_COL32(0, 255, 0, 255), text);

    const char* ready = "GET READY...";
    ImVec2 readySize = ImGui::CalcTextSize(ready);
    drawList->AddText(ImVec2(centerX - readySize.x / 2, height_ * 0.5f),
        IM_COL32(255, 255, 255, 255), ready);

    ImGui::End();
}

void Application::renderChallengeSelect() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Select", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float centerX = width_ / 2.0f;

    ImGui::SetCursorPos(ImVec2(centerX - 150, 100));
    ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "SELECT CHALLENGE");

    // Challenge buttons
    float buttonWidth = 400.0f;
    float buttonHeight = 100.0f;
    float buttonX = (width_ - buttonWidth) / 2.0f;

    ImGui::SetCursorPos(ImVec2(buttonX, 300));
    if (ImGui::Button("ACCURACY CHALLENGE\nHit the targets!", ImVec2(buttonWidth, buttonHeight))) {
        transitionTo(GameState::Countdown);
    }

    ImGui::SetCursorPos(ImVec2(buttonX, 450));
    if (ImGui::Button("POWER CHALLENGE\nKick as hard as you can!", ImVec2(buttonWidth, buttonHeight))) {
        transitionTo(GameState::Countdown);
    }

    ImGui::SetCursorPos(ImVec2(buttonX, 600));
    if (ImGui::Button("PENALTY SHOOTOUT\nBeat the goalkeeper!", ImVec2(buttonWidth, buttonHeight))) {
        transitionTo(GameState::Countdown);
    }

    ImGui::End();
}

void Application::renderCountdown() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Countdown", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    auto elapsed = std::chrono::steady_clock::now() - stateStartTime_;
    int remaining = 3 - static_cast<int>(std::chrono::duration<float>(elapsed).count());

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;
    float centerY = height_ / 2.0f;

    char buf[16];
    if (remaining > 0) {
        snprintf(buf, sizeof(buf), "%d", remaining);
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        drawList->AddText(nullptr, 200.0f, ImVec2(centerX - 50, centerY - 100),
            IM_COL32(255, 255, 0, 255), buf);
    } else {
        drawList->AddText(nullptr, 150.0f, ImVec2(centerX - 100, centerY - 75),
            IM_COL32(0, 255, 0, 255), "GO!");
    }

    if (remaining <= 0) {
        transitionTo(GameState::Playing);
    }

    ImGui::End();
}

void Application::renderGameplay() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Gameplay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    // Goal visualization area
    renderGoalVisualization();

    // Score display
    renderScoreDisplay();

    // Power meter (demo animation)
    float demoTime = static_cast<float>(ImGui::GetTime());
    float demoPower = 0.5f + 0.3f * sinf(demoTime * 2.0f);
    renderPowerMeter(demoPower);

    // Demo skeleton
    renderDemoSkeleton();

    ImGui::End();
}

void Application::renderResults() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Results", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    float centerX = width_ / 2.0f;

    ImGui::SetCursorPos(ImVec2(centerX - 100, 150));
    ImGui::TextColored(ImVec4(1, 0.85f, 0, 1), "RESULTS");

    ImGui::SetCursorPos(ImVec2(centerX - 150, 350));
    ImGui::Text("Score: 2500");

    ImGui::SetCursorPos(ImVec2(centerX - 150, 420));
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Grade: A");

    ImGui::SetCursorPos(ImVec2(centerX - 150, 490));
    ImGui::Text("Accuracy: 85%%");

    ImGui::SetCursorPos(ImVec2(centerX - 150, 530));
    ImGui::Text("Max Power: 95 km/h");

    float buttonWidth = 250.0f;
    ImGui::SetCursorPos(ImVec2((width_ - buttonWidth) / 2.0f, height_ - 250));
    if (ImGui::Button("PLAY AGAIN", ImVec2(buttonWidth, 80))) {
        transitionTo(GameState::SelectingChallenge);
    }

    ImGui::SetCursorPos(ImVec2((width_ - buttonWidth) / 2.0f, height_ - 150));
    if (ImGui::Button("MAIN MENU", ImVec2(buttonWidth, 80))) {
        transitionTo(GameState::Attract);
    }

    ImGui::End();
}

void Application::renderCelebration() {
    renderResults();
}

void Application::renderError() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Error", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;
    float centerY = height_ / 2.0f;

    // Friendly error message
    const char* message = "Oops! Please wait...";
    ImVec2 messageSize = ImGui::CalcTextSize(message);

    drawList->AddText(nullptr, 48.0f, ImVec2(centerX - messageSize.x, centerY - 60),
        kinect::theme::colors::CORAL, message);

    // Loading indicator (spinning circle)
    float time = static_cast<float>(ImGui::GetTime());
    float angle = time * 3.0f;
    float radius = 40.0f;
    int segments = 12;

    for (int i = 0; i < segments; i++) {
        float segmentAngle = (3.14159f * 2.0f * i / segments) + angle;
        float alpha = (float)(i + 1) / segments;
        ImU32 color = IM_COL32(0, 212, 171, static_cast<int>(255 * alpha));

        float x = centerX + cosf(segmentAngle) * radius;
        float y = centerY + 40 + sinf(segmentAngle) * radius;

        drawList->AddCircleFilled(ImVec2(x, y), 6.0f, color);
    }

    ImGui::End();
}

// UI helpers
void Application::renderGoalVisualization() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float goalX = 90.0f;
    float goalY = 60.0f;
    float goalW = static_cast<float>(width_) - 180.0f;
    float goalH = 500.0f;

    // Net diagonal line pattern in background
    ImU32 netColor = IM_COL32(255, 255, 255, 30);
    float netSpacing = 40.0f;
    for (float i = 0; i < goalW + goalH; i += netSpacing) {
        // Diagonal lines
        drawList->AddLine(
            ImVec2(goalX + i, goalY),
            ImVec2(goalX, goalY + i),
            netColor, 1.0f
        );
        drawList->AddLine(
            ImVec2(goalX + goalW - i, goalY),
            ImVec2(goalX + goalW, goalY + i),
            netColor, 1.0f
        );
    }

    // Goal frame with post thickness (20px width posts)
    float postThickness = 20.0f;

    // Post shadows (for depth)
    float shadowOffset = 4.0f;
    drawList->AddRectFilled(
        ImVec2(goalX + shadowOffset, goalY + shadowOffset),
        ImVec2(goalX + postThickness + shadowOffset, goalY + goalH + shadowOffset),
        kinect::theme::colors::GOAL_POST_SHADOW
    );
    drawList->AddRectFilled(
        ImVec2(goalX + goalW - postThickness + shadowOffset, goalY + shadowOffset),
        ImVec2(goalX + goalW + shadowOffset, goalY + goalH + shadowOffset),
        kinect::theme::colors::GOAL_POST_SHADOW
    );
    drawList->AddRectFilled(
        ImVec2(goalX + shadowOffset, goalY + shadowOffset),
        ImVec2(goalX + goalW + shadowOffset, goalY + postThickness + shadowOffset),
        kinect::theme::colors::GOAL_POST_SHADOW
    );

    // Goal posts
    drawList->AddRectFilled(
        ImVec2(goalX, goalY),
        ImVec2(goalX + postThickness, goalY + goalH),
        kinect::theme::colors::GOAL_POST
    );
    drawList->AddRectFilled(
        ImVec2(goalX + goalW - postThickness, goalY),
        ImVec2(goalX + goalW, goalY + goalH),
        kinect::theme::colors::GOAL_POST
    );
    drawList->AddRectFilled(
        ImVec2(goalX, goalY),
        ImVec2(goalX + goalW, goalY + postThickness),
        kinect::theme::colors::GOAL_POST
    );

    // 3x3 grid
    float cellW = goalW / 3.0f;
    float cellH = goalH / 3.0f;

    for (int i = 1; i < 3; i++) {
        // Vertical
        drawList->AddLine(
            ImVec2(goalX + i * cellW, goalY),
            ImVec2(goalX + i * cellW, goalY + goalH),
            IM_COL32(255, 255, 255, 80), 2.0f
        );
        // Horizontal
        drawList->AddLine(
            ImVec2(goalX, goalY + i * cellH),
            ImVec2(goalX + goalW, goalY + i * cellH),
            IM_COL32(255, 255, 255, 80), 2.0f
        );
    }

    // Highlight a random zone
    float time = static_cast<float>(ImGui::GetTime());
    int highlightCol = static_cast<int>(time) % 3;
    int highlightRow = (static_cast<int>(time) / 3) % 3;

    float pulse = 0.5f + 0.5f * sinf(time * 4.0f);
    ImU32 highlightColor = IM_COL32(0, static_cast<int>(255 * pulse), 0, 100);

    drawList->AddRectFilled(
        ImVec2(goalX + highlightCol * cellW, goalY + highlightRow * cellH),
        ImVec2(goalX + (highlightCol + 1) * cellW, goalY + (highlightRow + 1) * cellH),
        highlightColor
    );

    // Crosshair animation in target zone center
    float targetCenterX = goalX + (highlightCol + 0.5f) * cellW;
    float targetCenterY = goalY + (highlightRow + 0.5f) * cellH;
    float crosshairSize = 30.0f;
    float crosshairPulse = 0.7f + 0.3f * sinf(time * 8.0f);
    ImU32 crosshairColor = IM_COL32(0, 212, 171, static_cast<int>(255 * crosshairPulse));

    drawList->AddLine(
        ImVec2(targetCenterX - crosshairSize, targetCenterY),
        ImVec2(targetCenterX + crosshairSize, targetCenterY),
        crosshairColor, 3.0f
    );
    drawList->AddLine(
        ImVec2(targetCenterX, targetCenterY - crosshairSize),
        ImVec2(targetCenterX, targetCenterY + crosshairSize),
        crosshairColor, 3.0f
    );
    drawList->AddCircle(ImVec2(targetCenterX, targetCenterY), 15.0f, crosshairColor, 0, 2.0f);
}

void Application::renderPowerMeter(float power) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float meterX = 50.0f;
    float meterY = 600.0f;
    float meterW = 60.0f;
    float meterH = 350.0f;

    // Background
    drawList->AddRectFilled(
        ImVec2(meterX, meterY),
        ImVec2(meterX + meterW, meterY + meterH),
        IM_COL32(30, 30, 30, 255)
    );

    // Fill
    float fillH = meterH * power;
    ImU32 fillColor = IM_COL32(
        static_cast<int>(255 * power),
        static_cast<int>(255 * (1.0f - power * 0.5f)),
        0, 255
    );

    drawList->AddRectFilled(
        ImVec2(meterX, meterY + meterH - fillH),
        ImVec2(meterX + meterW, meterY + meterH),
        fillColor
    );

    // Border
    drawList->AddRect(
        ImVec2(meterX, meterY),
        ImVec2(meterX + meterW, meterY + meterH),
        IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f
    );

    // Label
    char label[32];
    snprintf(label, sizeof(label), "%d%%", static_cast<int>(power * 100));
    drawList->AddText(ImVec2(meterX, meterY + meterH + 10),
        IM_COL32(255, 255, 255, 255), label);
}

void Application::renderPlayerSkeleton(const core::BodyData& body) {
    // Not used in demo mode
}

void Application::renderDemoSkeleton() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float centerX = width_ / 2.0f;
    float centerY = static_cast<float>(displayConfig_.zones.controlsTop) + 150.0f;

    float time = static_cast<float>(ImGui::GetTime());

    // Animated demo skeleton - use selected jersey color
    ImU32 jointColor = kinect::theme::colors::JOINT;
    ImU32 boneColor = getJerseyColor(selectedJersey_);
    ImU32 kickFootColor = kinect::theme::colors::KICK_FOOT;
    ImU32 glowColor = getJerseyGlowColor(selectedJersey_);

    // Body center
    ImVec2 pelvis(centerX, centerY);
    ImVec2 spine(centerX, centerY - 60);
    ImVec2 chest(centerX, centerY - 100);
    ImVec2 head(centerX, centerY - 150);

    // Legs (animated)
    float legSwing = sinf(time * 3.0f) * 30.0f;
    ImVec2 leftHip(centerX - 30, centerY);
    ImVec2 leftKnee(centerX - 40 - legSwing, centerY + 80);
    ImVec2 leftFoot(centerX - 35 - legSwing * 2, centerY + 160);

    ImVec2 rightHip(centerX + 30, centerY);
    ImVec2 rightKnee(centerX + 40 + legSwing, centerY + 80);
    ImVec2 rightFoot(centerX + 35 + legSwing * 2, centerY + 160);

    // Arms
    ImVec2 leftShoulder(centerX - 50, centerY - 90);
    ImVec2 leftElbow(centerX - 80, centerY - 50);
    ImVec2 leftHand(centerX - 90, centerY - 10);

    ImVec2 rightShoulder(centerX + 50, centerY - 90);
    ImVec2 rightElbow(centerX + 80, centerY - 50);
    ImVec2 rightHand(centerX + 90, centerY - 10);

    // Draw glow layer under bones
    drawList->AddLine(pelvis, spine, glowColor, 8.0f);
    drawList->AddLine(spine, chest, glowColor, 8.0f);
    drawList->AddLine(chest, head, glowColor, 8.0f);

    drawList->AddLine(pelvis, leftHip, glowColor, 8.0f);
    drawList->AddLine(leftHip, leftKnee, glowColor, 8.0f);
    drawList->AddLine(leftKnee, leftFoot, glowColor, 8.0f);

    drawList->AddLine(pelvis, rightHip, glowColor, 8.0f);
    drawList->AddLine(rightHip, rightKnee, glowColor, 8.0f);
    drawList->AddLine(rightKnee, rightFoot, glowColor, 8.0f);

    drawList->AddLine(chest, leftShoulder, glowColor, 8.0f);
    drawList->AddLine(leftShoulder, leftElbow, glowColor, 8.0f);
    drawList->AddLine(leftElbow, leftHand, glowColor, 8.0f);

    drawList->AddLine(chest, rightShoulder, glowColor, 8.0f);
    drawList->AddLine(rightShoulder, rightElbow, glowColor, 8.0f);
    drawList->AddLine(rightElbow, rightHand, glowColor, 8.0f);

    // Draw bones
    drawList->AddLine(pelvis, spine, boneColor, 4.0f);
    drawList->AddLine(spine, chest, boneColor, 4.0f);
    drawList->AddLine(chest, head, boneColor, 4.0f);

    drawList->AddLine(pelvis, leftHip, boneColor, 4.0f);
    drawList->AddLine(leftHip, leftKnee, boneColor, 4.0f);
    drawList->AddLine(leftKnee, leftFoot, boneColor, 4.0f);

    drawList->AddLine(pelvis, rightHip, boneColor, 4.0f);
    drawList->AddLine(rightHip, rightKnee, boneColor, 4.0f);
    drawList->AddLine(rightKnee, rightFoot, boneColor, 4.0f);

    drawList->AddLine(chest, leftShoulder, boneColor, 4.0f);
    drawList->AddLine(leftShoulder, leftElbow, boneColor, 4.0f);
    drawList->AddLine(leftElbow, leftHand, boneColor, 4.0f);

    drawList->AddLine(chest, rightShoulder, boneColor, 4.0f);
    drawList->AddLine(rightShoulder, rightElbow, boneColor, 4.0f);
    drawList->AddLine(rightElbow, rightHand, boneColor, 4.0f);

    // Draw joints
    float r = 8.0f;
    drawList->AddCircleFilled(pelvis, r, jointColor);
    drawList->AddCircleFilled(spine, r, jointColor);
    drawList->AddCircleFilled(chest, r, jointColor);
    drawList->AddCircleFilled(head, r * 1.5f, jointColor);

    drawList->AddCircleFilled(leftHip, r, jointColor);
    drawList->AddCircleFilled(leftKnee, r, jointColor);
    drawList->AddCircleFilled(leftFoot, r, kickFootColor); // Kick foot

    drawList->AddCircleFilled(rightHip, r, jointColor);
    drawList->AddCircleFilled(rightKnee, r, jointColor);
    drawList->AddCircleFilled(rightFoot, r, jointColor);

    drawList->AddCircleFilled(leftShoulder, r, jointColor);
    drawList->AddCircleFilled(leftElbow, r, jointColor);
    drawList->AddCircleFilled(leftHand, r, jointColor);

    drawList->AddCircleFilled(rightShoulder, r, jointColor);
    drawList->AddCircleFilled(rightElbow, r, jointColor);
    drawList->AddCircleFilled(rightHand, r, jointColor);
}

void Application::renderScoreDisplay() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    auto elapsed = std::chrono::steady_clock::now() - stateStartTime_;
    int remaining = 60 - static_cast<int>(std::chrono::duration<float>(elapsed).count());
    if (remaining < 0) remaining = 0;

    // Panel background
    float panelX = width_ - 320.0f;
    float panelY = 60.0f;
    float panelW = 260.0f;
    float panelH = 180.0f;

    drawList->AddRectFilled(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        kinect::theme::colors::PANEL_BG
    );

    // Border
    drawList->AddRect(
        ImVec2(panelX, panelY),
        ImVec2(panelX + panelW, panelY + panelH),
        kinect::theme::colors::BORDER, 0.0f, 0, 2.0f
    );

    // Time display (48pt font size - scale up from base)
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "%d", remaining);

    // Color coding for low time (red when under 10 seconds)
    ImU32 timeColor = remaining < 10 ? kinect::theme::colors::CORAL : IM_COL32(255, 255, 255, 255);

    drawList->AddText(nullptr, 48.0f, ImVec2(panelX + 20, panelY + 20), timeColor, timeStr);

    const char* timeLabel = "SECONDS";
    drawList->AddText(ImVec2(panelX + 20, panelY + 75), IM_COL32(180, 180, 180, 255), timeLabel);

    // Score display (40pt font size)
    int demoScore = static_cast<int>(std::chrono::duration<float>(elapsed).count()) * 50;
    char scoreStr[32];
    snprintf(scoreStr, sizeof(scoreStr), "%d", demoScore);

    drawList->AddText(nullptr, 40.0f, ImVec2(panelX + 20, panelY + 100), kinect::theme::colors::GOLD, scoreStr);

    const char* scoreLabel = "POINTS";
    drawList->AddText(ImVec2(panelX + 20, panelY + 145), IM_COL32(180, 180, 180, 255), scoreLabel);
}

// Player customization helpers
ImU32 Application::getJerseyColor(JerseyColor jersey) const {
    switch (jersey) {
        case JerseyColor::TEAL:  return kinect::theme::jerseys::TEAL;
        case JerseyColor::CORAL: return kinect::theme::jerseys::CORAL;
        case JerseyColor::GOLD:  return kinect::theme::jerseys::GOLD;
        default: return kinect::theme::jerseys::TEAL;
    }
}

ImU32 Application::getJerseyGlowColor(JerseyColor jersey) const {
    switch (jersey) {
        case JerseyColor::TEAL:  return kinect::theme::jerseys::TEAL_GLOW;
        case JerseyColor::CORAL: return kinect::theme::jerseys::CORAL_GLOW;
        case JerseyColor::GOLD:  return kinect::theme::jerseys::GOLD_GLOW;
        default: return kinect::theme::jerseys::TEAL_GLOW;
    }
}

void Application::getBackgroundClearColor(BackgroundTheme theme, float* out) const {
    if (theme == BackgroundTheme::DAY) {
        kinect::theme::backgrounds::getDayColor(out);
    } else {
        kinect::theme::backgrounds::getNightColor(out);
    }
}

void Application::renderMiniSkeleton(ImDrawList* drawList, ImVec2 center, float scale, ImU32 color) {
    float s = scale;

    // Body
    ImVec2 head(center.x, center.y - 60 * s);
    ImVec2 neck(center.x, center.y - 45 * s);
    ImVec2 chest(center.x, center.y - 20 * s);
    ImVec2 pelvis(center.x, center.y + 10 * s);

    // Arms
    ImVec2 lShoulder(center.x - 25 * s, center.y - 35 * s);
    ImVec2 rShoulder(center.x + 25 * s, center.y - 35 * s);
    ImVec2 lHand(center.x - 40 * s, center.y);
    ImVec2 rHand(center.x + 40 * s, center.y);

    // Legs
    ImVec2 lFoot(center.x - 20 * s, center.y + 70 * s);
    ImVec2 rFoot(center.x + 20 * s, center.y + 70 * s);

    // Draw bones
    float thickness = 3.0f * s;
    drawList->AddLine(head, neck, color, thickness);
    drawList->AddLine(neck, chest, color, thickness);
    drawList->AddLine(chest, pelvis, color, thickness);
    drawList->AddLine(chest, lShoulder, color, thickness);
    drawList->AddLine(chest, rShoulder, color, thickness);
    drawList->AddLine(lShoulder, lHand, color, thickness);
    drawList->AddLine(rShoulder, rHand, color, thickness);
    drawList->AddLine(pelvis, lFoot, color, thickness);
    drawList->AddLine(pelvis, rFoot, color, thickness);

    // Draw joints
    float jointRadius = 4.0f * s;
    drawList->AddCircleFilled(head, jointRadius * 1.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircleFilled(lHand, jointRadius, color);
    drawList->AddCircleFilled(rHand, jointRadius, color);
    drawList->AddCircleFilled(lFoot, jointRadius, color);
    drawList->AddCircleFilled(rFoot, jointRadius, color);
}

void Application::renderOptionsSelect() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Options", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;
    float time = static_cast<float>(ImGui::GetTime());

    // Title
    const char* title = "CUSTOMIZE YOUR PLAYER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(nullptr, 48.0f, ImVec2(centerX - titleSize.x * 1.2f, 120),
        kinect::theme::colors::WHITE, title);

    // ========== JERSEY SELECTION ==========
    const char* jerseyLabel = "SELECT JERSEY COLOR";
    ImVec2 jerseyLabelSize = ImGui::CalcTextSize(jerseyLabel);
    drawList->AddText(ImVec2(centerX - jerseyLabelSize.x / 2, 280),
        kinect::theme::colors::GRAY, jerseyLabel);

    // Jersey buttons
    float buttonW = 280.0f;
    float buttonH = 120.0f;
    float buttonSpacing = 30.0f;
    float totalButtonsW = buttonW * 3 + buttonSpacing * 2;
    float buttonStartX = centerX - totalButtonsW / 2;
    float buttonY = 360.0f;

    struct JerseyOption {
        const char* name;
        JerseyColor color;
        ImU32 displayColor;
    };

    JerseyOption jerseys[] = {
        {"TEAL", JerseyColor::TEAL, kinect::theme::jerseys::TEAL},
        {"CORAL", JerseyColor::CORAL, kinect::theme::jerseys::CORAL},
        {"GOLD", JerseyColor::GOLD, kinect::theme::jerseys::GOLD}
    };

    for (int i = 0; i < 3; i++) {
        float bx = buttonStartX + i * (buttonW + buttonSpacing);
        ImVec2 bMin(bx, buttonY);
        ImVec2 bMax(bx + buttonW, buttonY + buttonH);

        bool isSelected = (selectedJersey_ == jerseys[i].color);
        bool isHovered = ImGui::IsMouseHoveringRect(bMin, bMax);

        // Button background
        ImU32 bgColor = isSelected ? jerseys[i].displayColor : IM_COL32(30, 40, 60, 200);
        drawList->AddRectFilled(bMin, bMax, bgColor, 12.0f);

        // Border
        ImU32 borderColor = isSelected ? kinect::theme::colors::WHITE :
                           (isHovered ? jerseys[i].displayColor : kinect::theme::colors::BORDER_SUBTLE);
        drawList->AddRect(bMin, bMax, borderColor, 12.0f, 0, isSelected ? 4.0f : 2.0f);

        // Jersey color preview square
        float previewSize = 40.0f;
        ImVec2 previewMin(bx + 20, buttonY + (buttonH - previewSize) / 2);
        ImVec2 previewMax(previewMin.x + previewSize, previewMin.y + previewSize);
        drawList->AddRectFilled(previewMin, previewMax, jerseys[i].displayColor, 6.0f);

        // Label
        ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : kinect::theme::colors::WHITE;
        drawList->AddText(ImVec2(bx + 80, buttonY + (buttonH - 20) / 2), textColor, jerseys[i].name);

        // Handle click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            selectedJersey_ = jerseys[i].color;
        }
    }

    // ========== SKELETON PREVIEW ==========
    float previewY = 650.0f;
    ImU32 jerseyColor = getJerseyColor(selectedJersey_);

    // Preview background
    float previewBgW = 400.0f;
    float previewBgH = 300.0f;
    ImVec2 previewBgMin(centerX - previewBgW / 2, previewY - 20);
    ImVec2 previewBgMax(centerX + previewBgW / 2, previewY + previewBgH);

    ImU32 previewBgColor = (selectedBackground_ == BackgroundTheme::DAY) ?
        IM_COL32(135, 206, 235, 200) : IM_COL32(10, 22, 40, 200);
    drawList->AddRectFilled(previewBgMin, previewBgMax, previewBgColor, 12.0f);
    drawList->AddRect(previewBgMin, previewBgMax, kinect::theme::colors::BORDER, 12.0f);

    // Render mini skeleton
    renderMiniSkeleton(drawList, ImVec2(centerX, previewY + 140), 1.8f, jerseyColor);

    // Preview label
    const char* previewLabel = "PREVIEW";
    ImVec2 previewLabelSize = ImGui::CalcTextSize(previewLabel);
    drawList->AddText(ImVec2(centerX - previewLabelSize.x / 2, previewY + previewBgH + 10),
        kinect::theme::colors::GRAY, previewLabel);

    // ========== BACKGROUND SELECTION ==========
    float bgSectionY = 1050.0f;
    const char* bgLabel = "SELECT TIME OF DAY";
    ImVec2 bgLabelSize = ImGui::CalcTextSize(bgLabel);
    drawList->AddText(ImVec2(centerX - bgLabelSize.x / 2, bgSectionY),
        kinect::theme::colors::GRAY, bgLabel);

    // Background toggle buttons
    float bgButtonW = 380.0f;
    float bgButtonH = 120.0f;
    float bgSpacing = 40.0f;
    float bgStartX = centerX - (bgButtonW * 2 + bgSpacing) / 2;
    float bgButtonY = bgSectionY + 80;

    struct BgOption {
        const char* name;
        BackgroundTheme theme;
        ImU32 previewColor;
    };

    BgOption backgrounds[] = {
        {"DAY", BackgroundTheme::DAY, IM_COL32(135, 206, 235, 255)},
        {"NIGHT", BackgroundTheme::NIGHT, IM_COL32(10, 22, 40, 255)}
    };

    for (int i = 0; i < 2; i++) {
        float bx = bgStartX + i * (bgButtonW + bgSpacing);
        ImVec2 bMin(bx, bgButtonY);
        ImVec2 bMax(bx + bgButtonW, bgButtonY + bgButtonH);

        bool isSelected = (selectedBackground_ == backgrounds[i].theme);
        bool isHovered = ImGui::IsMouseHoveringRect(bMin, bMax);

        // Button background with theme preview
        drawList->AddRectFilled(bMin, bMax, backgrounds[i].previewColor, 12.0f);

        // Overlay for non-selected
        if (!isSelected) {
            drawList->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 100), 12.0f);
        }

        // Border
        ImU32 borderColor = isSelected ? kinect::theme::colors::TEAL :
                           (isHovered ? kinect::theme::colors::TEAL : kinect::theme::colors::BORDER_SUBTLE);
        drawList->AddRect(bMin, bMax, borderColor, 12.0f, 0, isSelected ? 4.0f : 2.0f);

        // Label
        float textX = bx + (bgButtonW - 60) / 2;
        drawList->AddText(nullptr, 32.0f, ImVec2(textX, bgButtonY + (bgButtonH - 32) / 2),
            kinect::theme::colors::WHITE, backgrounds[i].name);

        // Handle click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            selectedBackground_ = backgrounds[i].theme;
        }
    }

    // ========== CONTINUE BUTTON ==========
    float continueY = 1700.0f;
    float continueW = 500.0f;
    float continueH = 100.0f;
    ImVec2 continueMin(centerX - continueW / 2, continueY);
    ImVec2 continueMax(centerX + continueW / 2, continueY + continueH);

    bool continueHovered = ImGui::IsMouseHoveringRect(continueMin, continueMax);

    // Pulsing effect
    float pulse = 0.8f + 0.2f * sinf(time * 3.0f);
    ImU32 continueBg = continueHovered ?
        kinect::theme::colors::TEAL_HOVER :
        IM_COL32(0, static_cast<int>(212 * pulse), static_cast<int>(170 * pulse), 255);

    drawList->AddRectFilled(continueMin, continueMax, continueBg, 16.0f);
    drawList->AddRect(continueMin, continueMax, kinect::theme::colors::WHITE, 16.0f, 0, 2.0f);

    const char* continueText = "CONTINUE";
    ImVec2 continueTextSize = ImGui::CalcTextSize(continueText);
    drawList->AddText(nullptr, 40.0f,
        ImVec2(centerX - continueTextSize.x * 1.1f, continueY + (continueH - 40) / 2),
        IM_COL32(0, 0, 0, 255), continueText);

    // Handle continue click
    if (continueHovered && ImGui::IsMouseClicked(0)) {
        transitionTo(GameState::SelectingChallenge);
    }

    // ========== TIMEOUT INDICATOR ==========
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - stateStartTime_).count();
    float remaining = 30.0f - elapsed;

    if (remaining < 10.0f && remaining > 0) {
        char timeoutStr[32];
        snprintf(timeoutStr, sizeof(timeoutStr), "Auto-continue in %d...", static_cast<int>(remaining) + 1);
        ImVec2 timeoutSize = ImGui::CalcTextSize(timeoutStr);
        drawList->AddText(ImVec2(centerX - timeoutSize.x / 2, 1820),
            kinect::theme::colors::CORAL, timeoutStr);
    }

    ImGui::End();
}

// State management
void Application::transitionTo(GameState newState) {
    gameState_ = newState;
    stateStartTime_ = std::chrono::steady_clock::now();
    logInfo("Transitioned to state: " + std::to_string(static_cast<int>(newState)));
}

void Application::updateStateLogic() {
    auto elapsed = std::chrono::steady_clock::now() - stateStartTime_;
    float elapsedSec = std::chrono::duration<float>(elapsed).count();

    switch (gameState_) {
        case GameState::PlayerDetected:
            if (elapsedSec > 2.0f) {
                transitionTo(GameState::SelectingOptions);  // Go to options first
            }
            break;

        case GameState::SelectingOptions:
            // Auto-continue after 30 seconds with default selections
            if (elapsedSec > 30.0f) {
                transitionTo(GameState::SelectingChallenge);
            }
            break;

        case GameState::Playing:
            if (elapsedSec > 60.0f) {
                transitionTo(GameState::Results);
            }
            break;

        case GameState::Results:
            if (elapsedSec > 30.0f) {
                transitionTo(GameState::Attract);
            }
            break;

        default:
            break;
    }
}

// Logging
void Application::logInfo(const std::string& msg) {
    std::cout << "[INFO] " << msg << std::endl;
}

void Application::logError(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << std::endl;
}

void Application::logWarning(const std::string& msg) {
    std::cout << "[WARN] " << msg << std::endl;
}

// Stub implementations for thread functions (not used in demo mode)
void Application::captureThreadFunc() {}
void Application::analysisThreadFunc() {}

} // namespace gui
} // namespace kinect

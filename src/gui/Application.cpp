/**
 * @file Application.cpp
 * @brief Main application implementation for Kinect Football (Demo Mode)
 */

#include "Application.h"
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

    // Clear background
    float clearColor[4] = { 0.05f, 0.1f, 0.15f, 1.0f };
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
    d3dContext_->OMSetRenderTargets(1, &renderTarget_, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Present
    swapChain_->Present(1, 0);
}

void Application::onResize(int width, int height) {
    if (width == 0 || height == 0) return;

    width_ = width;
    height_ = height;

    cleanupRenderTarget();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
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
    ID3D11Texture2D* backBuffer;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    d3dDevice_->CreateRenderTargetView(backBuffer, nullptr, &renderTarget_);
    backBuffer->Release();
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
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;

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

    // Title
    const char* title = "KINECT FOOTBALL";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2(centerX - titleSize.x / 2, height_ * 0.25f),
        IM_COL32(0, 200, 255, 255), title);

    // Subtitle
    const char* subtitle = "FIFA 2026 SIMULATOR";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    drawList->AddText(ImVec2(centerX - subSize.x / 2, height_ * 0.32f),
        IM_COL32(255, 215, 0, 255), subtitle);

    // Instructions
    const char* instructions = "Press SPACE or step in front of camera";
    ImVec2 instrSize = ImGui::CalcTextSize(instructions);
    drawList->AddText(ImVec2(centerX - instrSize.x / 2, height_ * 0.6f),
        IM_COL32(200, 200, 200, 255), instructions);

    // Pulsing indicator
    float pulse = 0.5f + 0.5f * sinf(static_cast<float>(ImGui::GetTime()) * 3.0f);
    ImU32 pulseColor = IM_COL32(0, static_cast<int>(255 * pulse), 0, 255);
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

    float centerX = width_ / 2.0f;
    ImGui::SetCursorPos(ImVec2(centerX - 100, height_ / 2.0f));
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR");
    ImGui::SetCursorPos(ImVec2(centerX - 150, height_ / 2.0f + 50));
    ImGui::Text("Press F12 to restart Kinect");

    ImGui::End();
}

// UI helpers
void Application::renderGoalVisualization() {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float goalX = 90.0f;
    float goalY = 60.0f;
    float goalW = static_cast<float>(width_) - 180.0f;
    float goalH = 500.0f;

    // Goal frame
    ImU32 goalColor = IM_COL32(255, 255, 255, 255);
    drawList->AddRect(
        ImVec2(goalX, goalY),
        ImVec2(goalX + goalW, goalY + goalH),
        goalColor, 0.0f, 0, 4.0f
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

    // Animated demo skeleton
    ImU32 jointColor = IM_COL32(0, 255, 255, 255);
    ImU32 boneColor = IM_COL32(0, 200, 200, 200);

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
    drawList->AddCircleFilled(leftFoot, r, IM_COL32(255, 0, 255, 255)); // Kick foot

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

    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "TIME: %d", remaining);
    drawList->AddText(ImVec2(width_ - 200.0f, 100.0f),
        IM_COL32(255, 255, 255, 255), timeStr);

    int demoScore = static_cast<int>(std::chrono::duration<float>(elapsed).count()) * 50;
    char scoreStr[32];
    snprintf(scoreStr, sizeof(scoreStr), "SCORE: %d", demoScore);
    drawList->AddText(ImVec2(width_ - 200.0f, 140.0f),
        IM_COL32(255, 215, 0, 255), scoreStr);
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

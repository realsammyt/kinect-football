#pragma once

#include "core/KinectDevice.h"
#include "core/BodyTracker.h"
#include "core/PlayerTracker.h"
#include "core/RingBuffer.h"
#include "DisplayConfig.h"

#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <functional>

struct ImGuiContext;

namespace kinect {

// Forward declarations
namespace motion {
    class KickDetector;
    class KickAnalyzer;
}

namespace game {
    class GameManager;
}

namespace kiosk {
    class KioskManager;
    class SessionManager;
}

namespace gui {

/**
 * @brief Game state machine
 */
enum class GameState {
    Attract,            // Idle, showing attract loop
    PlayerDetected,     // Player entered, brief welcome
    SelectingChallenge, // Touch screen to select challenge
    Countdown,          // 3-2-1 countdown
    Playing,            // Active gameplay
    Results,            // Showing results
    Celebration,        // Goal celebration effects
    Error               // Error state
};

/**
 * @brief Main application class with 3-thread architecture
 *
 * Thread model (from kinect-native):
 * 1. Capture thread: Polls Kinect for frames
 * 2. Analysis thread: Processes body data, detects kicks
 * 3. Main thread: GUI rendering
 *
 * Uses ring buffer to decouple capture from analysis.
 */
class Application {
public:
    Application();
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /**
     * @brief Initialize application
     * @param hWnd Window handle
     * @param width Window width (1080 for portrait)
     * @param height Window height (1920 for portrait)
     * @return true if successful
     */
    bool initialize(HWND hWnd, int width, int height);

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Update game logic (called from main loop)
     */
    void update();

    /**
     * @brief Render frame (called from main loop)
     */
    void render();

    /**
     * @brief Handle window resize
     */
    void onResize(int width, int height);

    /**
     * @brief Handle key press
     */
    void onKeyDown(int key);

    /**
     * @brief Full Kinect restart (F12)
     * Uses join-before-destroy pattern from kinect-native
     */
    void onKinectRestart();

    // State queries
    GameState getGameState() const { return gameState_; }
    bool isRunning() const { return running_; }

private:
    // Window
    HWND hWnd_ = nullptr;
    int width_ = 1080;
    int height_ = 1920;

    // Display configuration (portrait mode)
    config::DisplayConfig displayConfig_;

    // DirectX 11
    ID3D11Device* d3dDevice_ = nullptr;
    ID3D11DeviceContext* d3dContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* renderTarget_ = nullptr;

    // ImGui
    ImGuiContext* imguiContext_ = nullptr;

    // Kinect components
    std::unique_ptr<core::KinectDevice> kinect_;
    std::unique_ptr<core::BodyTracker> tracker_;
    std::unique_ptr<core::PlayerTracker> playerTracker_;

    // Motion analysis
    std::unique_ptr<motion::KickDetector> kickDetector_;
    std::unique_ptr<motion::KickAnalyzer> kickAnalyzer_;

    // Game logic
    std::unique_ptr<game::GameManager> gameManager_;

    // Kiosk management
    std::unique_ptr<kiosk::KioskManager> kioskManager_;
    std::unique_ptr<kiosk::SessionManager> sessionManager_;

    // Threading (3-thread architecture from kinect-native)
    std::thread captureThread_;
    std::thread analysisThread_;
    std::atomic<bool> captureRunning_{false};
    std::atomic<bool> analysisRunning_{false};
    std::atomic<bool> running_{false};

    // Ring buffer for thread decoupling
    core::RingBuffer<core::BodyData, 30> bodyBuffer_;

    // Shared state with mutex protection
    std::mutex currentBodyMutex_;
    std::vector<core::BodyData> currentBodies_;

    // Game state
    GameState gameState_ = GameState::Attract;
    std::chrono::steady_clock::time_point stateStartTime_;
    float attractIdleTime_ = 0.0f;

    // Thread functions
    void captureThreadFunc();
    void analysisThreadFunc();

    // DirectX setup
    bool createD3DDevice();
    void cleanupD3DDevice();
    bool createRenderTarget();
    void cleanupRenderTarget();

    // ImGui setup
    bool initImGui();
    void cleanupImGui();

    // Rendering
    void renderAttractMode();
    void renderPlayerDetected();
    void renderChallengeSelect();
    void renderCountdown();
    void renderGameplay();
    void renderResults();
    void renderCelebration();
    void renderError();

    // UI helpers (portrait layout)
    void renderGoalVisualization();
    void renderPowerMeter(float power);
    void renderPlayerSkeleton(const core::BodyData& body);
    void renderScoreDisplay();

    // State transitions
    void transitionTo(GameState newState);
    void updateStateLogic();

    // Logging
    void logInfo(const std::string& msg);
    void logError(const std::string& msg);
    void logWarning(const std::string& msg);
};

} // namespace gui
} // namespace kinect

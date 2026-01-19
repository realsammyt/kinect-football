# Kinect Football - Core Kiosk Framework

This document describes the core kiosk application framework created for the FIFA 2026 Soccer Simulator.

## Architecture Overview

The application uses a **3-thread architecture** for optimal performance:

1. **Capture Thread** - Grabs frames from Azure Kinect at 30 FPS
2. **Analysis Thread** - Processes body tracking and motion detection
3. **Main Thread** - Renders GUI and handles user interaction

### Threading Pattern

```
┌─────────────────┐      ┌──────────────────┐      ┌─────────────────┐
│  Capture Thread │─────►│ Analysis Thread  │─────►│  Main Thread    │
│  (Kinect FPS)   │      │ (Motion Detect)  │      │  (GUI Render)   │
└─────────────────┘      └──────────────────┘      └─────────────────┘
        │                         │                          │
   captureBuffer_            renderBuffer_              Display
   (Ring Buffer)             (Ring Buffer)
```

## Files Created

### 1. `include/common.h`

Shared data structures and utilities:

- **GameState** - State machine enum (Attract, PlayerDetected, Playing, Results, etc.)
- **ChallengeType** - Challenge types (Accuracy, Power, PenaltyShootout, etc.)
- **KickType** - Kick classification (Instep, Inside, Outside, Volley, Chip)
- **PlayerData** - Skeleton data with velocity tracking
- **KickData** - Kick detection result with trajectory prediction
- **SessionData** - Complete session information
- **FrameData** - Container for Kinect frames and body tracking
- **HealthMetrics** - System health monitoring
- **Utility functions** - Vector math, timestamps, etc.

Key constants:
```cpp
constexpr int MAX_BODIES = 6;
constexpr float KICK_DETECTION_THRESHOLD = 2.0f; // m/s
constexpr float SESSION_TIMEOUT_SECONDS = 60.0f;
constexpr float ATTRACT_MODE_IDLE_TIME = 30.0f;
```

### 2. `src/core/RingBuffer.h`

Thread-safe ring buffer template for inter-thread communication:

```cpp
RingBuffer<FrameData, 4> captureBuffer_;  // Capture -> Analysis
RingBuffer<FrameData, 4> renderBuffer_;   // Analysis -> Render
```

Features:
- Lock-based thread safety with mutex
- Move semantics for zero-copy transfers
- Power-of-2 size for optimal performance
- Bounded vectors for predictable allocation

### 3. `src/gui/Application.h/cpp`

Main application class implementing the 3-thread architecture:

**Key Methods:**
```cpp
bool initialize();                  // Setup Kinect and threads
void run();                        // Main loop (blocks)
void shutdown();                   // Graceful cleanup

// Thread entry points
void captureThreadFunc();          // Kinect capture loop
void analysisThreadFunc();         // Motion processing loop
void renderLoop();                 // GUI rendering loop

// State machine
void updateStateMachine(float deltaTime);
void transitionTo(GameState newState);

// Kinect recovery
void onKinectRestart();           // Hot-restart Kinect
```

**Thread Safety:**
- Uses `std::atomic<bool>` for running flags
- Mutex-protected state transitions
- Join-before-destroy pattern for safe shutdown
- Proper thread synchronization in `onKinectRestart()`

### 4. `src/kiosk/KioskManager.h/cpp`

System health monitoring and auto-recovery:

**Features:**
- Health check monitoring (every 5 seconds)
- Watchdog timer for hang detection (30 second timeout)
- Auto-recovery on errors (after 3 consecutive failures)
- Error logging and statistics

**Configuration:**
```cpp
KioskManager::Config config;
config.healthCheckIntervalSeconds = 5.0f;
config.watchdogTimeoutSeconds = 30.0f;
config.autoRestartDelaySeconds = 10.0f;
config.maxConsecutiveErrors = 3;
config.enableAutoRecovery = true;
config.enableWatchdog = true;
```

**Usage:**
```cpp
KioskManager manager;
manager.initialize(config);
manager.start();

// In main loop
manager.updateHealth(metrics);
manager.kickWatchdog();

// Set restart callback
manager.setRestartCallback([&app]() {
    app.onKinectRestart();
});
```

### 5. `src/kiosk/SessionManager.h/cpp`

Player session lifecycle management:

**Features:**
- Session start/end tracking
- Player identification and re-identification (5 second window)
- Session timeout management (60 seconds)
- Analytics collection
- Session data export (CSV/JSON)

**Analytics:**
```cpp
struct Analytics {
    uint64_t totalSessions;
    uint64_t completedSessions;
    uint64_t cancelledSessions;
    uint64_t sharedSessions;
    std::map<ChallengeType, uint64_t> challengeCounts;
    std::map<std::string, uint64_t> shareMethodCounts;
    float avgSessionDurationSeconds;
    float avgScore;
};
```

**Usage:**
```cpp
SessionManager sessions;
sessions.initialize(config);

// Start session
std::string sessionId = sessions.startSession(playerId);

// Update session
sessions.setChallenge(sessionId, ChallengeType::ACCURACY);

// End session
sessions.endSession(sessionId, result);

// Export data
sessions.exportSessions("./sessions/export.csv");
```

## State Machine

The application implements a state machine for the kiosk lifecycle:

```
ATTRACT
  │
  ├─ Player detected ────► PLAYER_DETECTED
  │                              │
  │                              ├─ 3 sec timeout ────► SELECTING_CHALLENGE
  │                              │                              │
  │                              │                              ├─ Challenge selected ────► COUNTDOWN
  │                              │                              │                                  │
  │                              │                              │                                  ├─ 3 sec ────► PLAYING
  │                              │                              │                                                      │
  │                              │                              │                                                      ├─ Complete ────► RESULTS
  │                              │                              │                                                                            │
  │                              │                              │                                                                            ├─ 10 sec ────► SHARE
  │                              │                              │                                                                                              │
  │                              │                              │                                                                                              ├─ Complete ────► THANK_YOU
  │                              │                              │                                                                                                                  │
  └──────────────────────────────┴──────────────────────────────┴──────────────────────────────────────────────────────────────────────────────────────────┘
                                                                                                                                                            5 sec
```

### State Transitions

Implemented in `Application::updateStateMachine()`:

```cpp
switch (state) {
    case GameState::ATTRACT:
        if (isPlayerPresent()) {
            transitionTo(GameState::PLAYER_DETECTED);
        }
        break;

    case GameState::PLAYER_DETECTED:
        if (stateTimer_ > 3.0f) {
            transitionTo(GameState::SELECTING_CHALLENGE);
        }
        break;

    // ... etc
}
```

## Thread Synchronization

### Join-Before-Destroy Pattern

From `kinect-native` best practices:

```cpp
void Application::shutdown() {
    running_ = false;     // Signal threads to stop
    joinThreadsSafely();  // Wait for threads to finish
    shutdownKinect();     // Clean up hardware
}

void Application::joinThreadsSafely() {
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    if (analysisThread_.joinable()) {
        analysisThread_.join();
    }
}
```

### Kinect Hot-Restart

The `onKinectRestart()` method safely restarts the Kinect without crashing:

```cpp
void Application::onKinectRestart() {
    bool wasRunning = running_.load();

    if (wasRunning) {
        running_ = false;
        waitForThreadsToStop();
    }

    shutdownKinect();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (initializeKinect()) {
        if (wasRunning) {
            running_ = true;
            captureThread_ = std::thread(&Application::captureThreadFunc, this);
            analysisThread_ = std::thread(&Application::analysisThreadFunc, this);
        }
    }
}
```

## Building

### Prerequisites

- Azure Kinect SDK v1.4.2+
- Azure Kinect Body Tracking SDK
- CMake 3.15+
- Visual Studio 2019+ (Windows)

### Build Commands

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### Running

```bash
# Console version (for testing)
./bin/kinect_football_console

# Windows GUI version
./bin/KinectFootball.exe
```

## Integration Points

### Adding New Challenges

1. Create challenge class inheriting from `ChallengeBase`
2. Add enum to `ChallengeType` in `common.h`
3. Implement in `src/game/`
4. Add state handling in `Application::updateStateMachine()`
5. Add rendering in `Application::renderGameplay()`

### Adding Analytics

1. Extend `SessionData` in `common.h`
2. Update `SessionManager::updateAnalytics()`
3. Modify `SessionManager::exportSessions()` format

### Custom Health Checks

1. Add metrics to `HealthMetrics` in `common.h`
2. Implement check in `KioskManager::performHealthCheck()`
3. Add recovery logic in `KioskManager::attemptRecovery()`

## Best Practices

### 1. Thread Safety
- Always use `std::lock_guard<std::mutex>` for shared state
- Use atomic types for simple flags
- Never hold locks while calling callbacks

### 2. Memory Management
- Use `BoundedVector` for predictable allocations
- Move semantics for frame transfers (zero-copy)
- RAII for Kinect handles

### 3. Error Handling
- Report errors via `KioskManager::reportError()`
- Log all state transitions
- Implement auto-recovery where possible

### 4. Performance
- Ring buffers are power-of-2 sized for performance
- Frame processing happens on dedicated thread
- GUI rendering is decoupled from capture

## Debugging

### Logging Macros

```cpp
LOG_INFO("Message");    // General information
LOG_WARN("Message");    // Warnings
LOG_ERROR("Message");   // Errors
LOG_DEBUG("Message");   // Debug info (verbose)
```

### Health Monitoring

```cpp
auto health = app.getHealthMetrics();
std::cout << "FPS: " << health.avgFps << std::endl;
std::cout << "Frames dropped: " << health.framesDropped << std::endl;
std::cout << "Kinect healthy: " << health.kinectHealthy << std::endl;
```

### Session Statistics

```cpp
auto analytics = sessionManager.getAnalytics();
std::cout << "Total sessions: " << analytics.totalSessions << std::endl;
std::cout << "Avg score: " << analytics.avgScore << "%" << std::endl;
std::cout << "Share rate: "
          << (100.0f * analytics.sharedSessions / analytics.totalSessions)
          << "%" << std::endl;
```

## Next Steps

The following components still need implementation:

1. **Motion Detection**
   - Implement `src/motion/KickDetector.cpp`
   - Velocity calculation from joint history
   - Kick type classification

2. **Challenges**
   - Implement `src/game/AccuracyChallenge.cpp`
   - Implement `src/game/PowerChallenge.cpp`
   - Implement `src/game/PenaltyShootout.cpp`

3. **GUI Rendering**
   - Implement render methods in `Application.cpp`
   - Create ImGui layouts for each state
   - Add animations and effects

4. **Sharing**
   - QR code generation
   - Email/SMS integration
   - Photo/video capture

5. **Audio**
   - Background music
   - Sound effects for kicks
   - Countdown audio

See the main documentation and `Photo/Video Booth Specialist` context for detailed implementations of these features.

## File Locations

```
kinect-football/
├── include/
│   └── common.h                    # Shared data structures
├── src/
│   ├── core/
│   │   └── RingBuffer.h           # Thread-safe ring buffer
│   ├── gui/
│   │   ├── Application.h          # Main application class
│   │   └── Application.cpp
│   ├── kiosk/
│   │   ├── KioskManager.h         # Health monitoring
│   │   ├── KioskManager.cpp
│   │   ├── SessionManager.h       # Session lifecycle
│   │   └── SessionManager.cpp
│   ├── main.cpp                   # Windows GUI entry point
│   └── main_console.cpp           # Console entry point
└── CMakeLists.txt                 # Build configuration
```

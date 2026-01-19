#include "Application.h"
#include <iostream>
#include <chrono>

Application::Application()
    : running_(false)
    , captureThreadReady_(false)
    , analysisThreadReady_(false)
    , currentState_(GameState::ATTRACT)
    , previousState_(GameState::ATTRACT)
    , stateTimer_(0.0f)
{
    // Initialize device config
    deviceConfig_ = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    deviceConfig_.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    deviceConfig_.color_resolution = K4A_COLOR_RESOLUTION_1080P;
    deviceConfig_.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    deviceConfig_.camera_fps = K4A_FRAMES_PER_SECOND_30;
    deviceConfig_.synchronized_images_only = true;
}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    LOG_INFO("Initializing application...");

    if (!initializeKinect()) {
        LOG_ERROR("Failed to initialize Kinect");
        return false;
    }

    LOG_INFO("Application initialized successfully");
    return true;
}

bool Application::initializeKinect() {
    try {
        // Open device
        const uint32_t deviceCount = k4a::device::get_installed_count();
        if (deviceCount == 0) {
            LOG_ERROR("No Azure Kinect devices found");
            return false;
        }

        LOG_INFO("Found " << deviceCount << " Kinect device(s)");
        device_ = k4a::device::open(K4A_DEVICE_DEFAULT);

        // Get calibration
        calibration_ = device_.get_calibration(
            deviceConfig_.depth_mode,
            deviceConfig_.color_resolution
        );

        // Create body tracker
        k4abt_tracker_configuration_t trackerConfig = K4ABT_TRACKER_CONFIG_DEFAULT;
        trackerConfig.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU;
        trackerConfig.sensor_orientation = K4ABT_SENSOR_ORIENTATION_DEFAULT;

        tracker_ = k4abt::tracker::create(calibration_, trackerConfig);
        if (!tracker_) {
            LOG_ERROR("Failed to create body tracker");
            return false;
        }

        // Start cameras
        device_.start_cameras(&deviceConfig_);

        health_.kinectHealthy = true;
        health_.trackerHealthy = true;

        LOG_INFO("Kinect initialized successfully");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Exception during Kinect initialization: " << e.what());
        return false;
    }
}

void Application::shutdownKinect() {
    LOG_INFO("Shutting down Kinect...");

    health_.kinectHealthy = false;
    health_.trackerHealthy = false;

    if (tracker_) {
        tracker_.destroy();
        tracker_ = nullptr;
    }

    if (device_) {
        device_.stop_cameras();
        device_.close();
    }

    LOG_INFO("Kinect shutdown complete");
}

void Application::onKinectRestart() {
    LOG_INFO("Restarting Kinect...");

    // Signal threads to pause
    bool wasRunning = running_.load();
    if (wasRunning) {
        running_ = false;
        waitForThreadsToStop();
    }

    // Shutdown and reinitialize
    shutdownKinect();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (initializeKinect()) {
        LOG_INFO("Kinect restarted successfully");

        // Restart threads if they were running
        if (wasRunning) {
            running_ = true;
            captureThread_ = std::thread(&Application::captureThreadFunc, this);
            analysisThread_ = std::thread(&Application::analysisThreadFunc, this);
        }
    } else {
        LOG_ERROR("Failed to restart Kinect");
        transitionTo(GameState::ERROR_STATE);
    }
}

void Application::run() {
    LOG_INFO("Starting application...");

    running_ = true;

    // Start worker threads
    captureThread_ = std::thread(&Application::captureThreadFunc, this);
    analysisThread_ = std::thread(&Application::analysisThreadFunc, this);

    // Wait for threads to be ready
    while (!captureThreadReady_ || !analysisThreadReady_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_INFO("All threads ready, starting main loop");

    // Main render loop
    renderLoop();
}

void Application::shutdown() {
    LOG_INFO("Shutting down application...");

    running_ = false;

    // Join threads safely
    joinThreadsSafely();

    // Shutdown hardware
    shutdownKinect();

    LOG_INFO("Application shutdown complete");
}

void Application::captureThreadFunc() {
    LOG_INFO("Capture thread started");
    captureThreadReady_ = true;

    auto lastLogTime = std::chrono::steady_clock::now();

    while (running_) {
        try {
            // Capture frame from device
            k4a::capture capture;
            if (!device_.get_capture(&capture, std::chrono::milliseconds(100))) {
                continue; // Timeout, try again
            }

            // Enqueue to body tracker
            if (!tracker_.enqueue_capture(capture)) {
                LOG_WARN("Failed to enqueue capture to tracker");
                health_.framesDropped++;
                continue;
            }

            // Pop result from tracker
            k4abt::frame bodyFrame = tracker_.pop_result(std::chrono::milliseconds(100));
            if (!bodyFrame) {
                continue; // No result yet
            }

            // Create frame data
            FrameData frameData;
            frameData.colorImage = capture.get_color_image();
            frameData.depthImage = capture.get_depth_image();
            frameData.bodyIndexMap = bodyFrame.get_body_index_map();
            frameData.bodyFrame = k4abt_frame_reference(bodyFrame.handle());
            frameData.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();

            // Push to analysis buffer
            if (!captureBuffer_.push(std::move(frameData))) {
                LOG_WARN("Capture buffer full, dropping frame");
                health_.framesDropped++;
            }

            health_.framesProcessed++;

            // Log stats every 5 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count() >= 5) {
                LOG_DEBUG("Capture: " << health_.framesProcessed.load() << " frames, "
                         << health_.framesDropped.load() << " dropped, "
                         << "buffer: " << captureBuffer_.fillPercentage() << "%");
                lastLogTime = now;
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Exception in capture thread: " << e.what());
            health_.kinectHealthy = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    LOG_INFO("Capture thread stopped");
}

void Application::analysisThreadFunc() {
    LOG_INFO("Analysis thread started");
    analysisThreadReady_ = true;

    while (running_) {
        auto frameOpt = captureBuffer_.pop();
        if (!frameOpt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        FrameData& frame = *frameOpt;

        try {
            // Process frame
            processFrame(frame);

            // Push to render buffer
            if (!renderBuffer_.push(std::move(frame))) {
                LOG_WARN("Render buffer full, dropping frame");
                health_.framesDropped++;
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Exception in analysis thread: " << e.what());
        }
    }

    LOG_INFO("Analysis thread stopped");
}

void Application::renderLoop() {
    LOG_INFO("Render loop started");

    auto lastFrameTime = std::chrono::steady_clock::now();

    while (running_) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;

        // Update FPS
        if (deltaTime > 0.0f) {
            float currentFps = 1.0f / deltaTime;
            health_.avgFps = 0.9f * health_.avgFps + 0.1f * currentFps;
        }

        // Update state machine
        updateStateMachine(deltaTime);

        // Get latest frame
        auto frameOpt = renderBuffer_.pop();
        if (frameOpt) {
            render(*frameOpt);
        } else {
            // No new frame, render with empty data
            FrameData emptyFrame;
            render(emptyFrame);
        }

        // Handle input
        handleKeyboard();

        // Check health
        updateHealthMetrics();

        // Limit frame rate (optional)
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    LOG_INFO("Render loop stopped");
}

void Application::processFrame(FrameData& frame) {
    // Extract player data from body frame
    extractPlayerData(frame.bodyFrame, frame);

    // Update velocities
    updatePlayerVelocities(frame);
}

void Application::extractPlayerData(k4abt_frame_t bodyFrame, FrameData& frame) {
    if (!bodyFrame) return;

    uint32_t numBodies = k4abt_frame_get_num_bodies(bodyFrame);

    frame.players.clear();
    frame.players.reserve(std::min(numBodies, static_cast<uint32_t>(MAX_BODIES)));

    for (uint32_t i = 0; i < numBodies && i < MAX_BODIES; ++i) {
        k4abt_body_t body;
        if (k4abt_frame_get_body_skeleton(bodyFrame, i, &body.skeleton) == K4A_RESULT_SUCCEEDED) {
            PlayerData player;
            player.id = k4abt_frame_get_body_id(bodyFrame, i);
            player.isActive = true;
            player.lastUpdateTime = frame.timestamp;

            // Copy joint data
            for (int j = 0; j < NUM_JOINTS; ++j) {
                const k4abt_joint_t& joint = body.skeleton.joints[j];
                player.joints[j].position = joint.position;
                player.joints[j].orientation = joint.orientation;
                player.joints[j].confidence = joint.confidence_level;
                player.joints[j].timestamp_us = frame.timestamp;
            }

            frame.players.push_back(player);
        }
    }

    // Update active players list (with mutex)
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        activePlayers_ = frame.players;
    }
}

void Application::updatePlayerVelocities(FrameData& frame) {
    // TODO: Calculate joint velocities from previous frames
    // This requires maintaining a history buffer
}

void Application::updateStateMachine(float deltaTime) {
    stateTimer_ += deltaTime;

    GameState state = getGameState();

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

        case GameState::SELECTING_CHALLENGE:
            checkTimeout();
            break;

        case GameState::COUNTDOWN:
            if (stateTimer_ >= 3.0f) {
                transitionTo(GameState::PLAYING);
            }
            break;

        case GameState::PLAYING:
            // Game logic handles transition
            break;

        case GameState::RESULTS:
            if (stateTimer_ > 10.0f) {
                transitionTo(GameState::SHARE);
            }
            break;

        case GameState::SHARE:
            checkTimeout();
            break;

        case GameState::THANK_YOU:
            if (stateTimer_ > 5.0f) {
                transitionTo(GameState::ATTRACT);
            }
            break;

        case GameState::ERROR_STATE:
            // Auto-recovery attempt
            if (stateTimer_ > 5.0f) {
                onKinectRestart();
                transitionTo(GameState::ATTRACT);
            }
            break;

        default:
            break;
    }
}

void Application::transitionTo(GameState newState) {
    if (newState == currentState_) return;

    LOG_INFO("State transition: " << static_cast<int>(currentState_)
             << " -> " << static_cast<int>(newState));

    onStateExit(currentState_);

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        previousState_ = currentState_;
        currentState_ = newState;
    }

    stateTimer_ = 0.0f;
    stateStartTime_ = std::chrono::steady_clock::now();
    onStateEnter(newState);
}

void Application::onStateEnter(GameState state) {
    switch (state) {
        case GameState::ATTRACT:
            resetSession();
            break;

        case GameState::PLAYER_DETECTED:
            currentSession_.startTime = std::chrono::system_clock::now();
            currentSession_.sessionId = util::generateSessionId();
            break;

        case GameState::PLAYING:
            // Start challenge
            break;

        default:
            break;
    }
}

void Application::onStateExit(GameState state) {
    switch (state) {
        case GameState::PLAYING:
            currentSession_.endTime = std::chrono::system_clock::now();
            break;

        default:
            break;
    }
}

void Application::render(const FrameData& frame) {
    GameState state = getGameState();

    switch (state) {
        case GameState::ATTRACT:
            renderAttractMode();
            break;
        case GameState::PLAYER_DETECTED:
            renderPlayerDetected();
            break;
        case GameState::SELECTING_CHALLENGE:
            renderChallengeSelection();
            break;
        case GameState::COUNTDOWN:
            renderCountdown();
            break;
        case GameState::PLAYING:
            renderGameplay(frame);
            break;
        case GameState::RESULTS:
            renderResults();
            break;
        case GameState::SHARE:
            renderShare();
            break;
        case GameState::THANK_YOU:
            renderThankYou();
            break;
        default:
            break;
    }
}

// Placeholder render functions
void Application::renderAttractMode() {
    // TODO: Implement attract mode rendering
}

void Application::renderPlayerDetected() {
    // TODO: Implement player detected rendering
}

void Application::renderChallengeSelection() {
    // TODO: Implement challenge selection rendering
}

void Application::renderCountdown() {
    // TODO: Implement countdown rendering
}

void Application::renderGameplay(const FrameData& frame) {
    // TODO: Implement gameplay rendering
}

void Application::renderResults() {
    // TODO: Implement results rendering
}

void Application::renderCelebration() {
    // TODO: Implement celebration rendering
}

void Application::renderShare() {
    // TODO: Implement share screen rendering
}

void Application::renderThankYou() {
    // TODO: Implement thank you rendering
}

void Application::handleKeyboard() {
    // TODO: Implement keyboard handling
}

void Application::handleGestures(const std::vector<PlayerData>& players) {
    // TODO: Implement gesture recognition
}

void Application::updateHealthMetrics() {
    auto now = std::chrono::steady_clock::now();
    health_.lastFrameTime = now;

    // Check for Kinect health
    if (!device_) {
        health_.kinectHealthy = false;
    }

    if (!tracker_) {
        health_.trackerHealthy = false;
    }
}

void Application::checkTimeout() {
    if (!isPlayerPresent() && stateTimer_ > SESSION_TIMEOUT_SECONDS) {
        LOG_INFO("Session timeout");
        transitionTo(GameState::THANK_YOU);
    }
}

bool Application::isPlayerPresent() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return !activePlayers_.empty();
}

void Application::resetSession() {
    currentSession_ = SessionData();
    activePlayers_.clear();
}

GameState Application::getGameState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentState_;
}

void Application::setGameState(GameState state) {
    transitionTo(state);
}

HealthMetrics Application::getHealthMetrics() const {
    return health_;
}

void Application::waitForThreadsToStop() {
    // Wait for threads to acknowledge shutdown
    int waitCount = 0;
    while ((captureThreadReady_ || analysisThreadReady_) && waitCount++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Application::joinThreadsSafely() {
    // Join capture thread
    if (captureThread_.joinable()) {
        LOG_INFO("Joining capture thread...");
        captureThread_.join();
        LOG_INFO("Capture thread joined");
    }

    // Join analysis thread
    if (analysisThread_.joinable()) {
        LOG_INFO("Joining analysis thread...");
        analysisThread_.join();
        LOG_INFO("Analysis thread joined");
    }

    captureThreadReady_ = false;
    analysisThreadReady_ = false;
}

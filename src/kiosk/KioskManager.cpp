#include "KioskManager.h"
#include <iostream>

KioskManager::KioskManager()
    : running_(false)
    , systemHealthy_(true)
    , consecutiveErrors_(0)
    , lastWatchdogKick_(0)
    , watchdogExpired_(false)
{
    stats_.startTime = std::chrono::system_clock::now();
}

KioskManager::~KioskManager() {
    stop();
}

bool KioskManager::initialize(const Config& config) {
    config_ = config;

    LOG_INFO("KioskManager initialized");
    LOG_INFO("  Health check interval: " << config_.healthCheckIntervalSeconds << "s");
    LOG_INFO("  Watchdog timeout: " << config_.watchdogTimeoutSeconds << "s");
    LOG_INFO("  Auto-recovery: " << (config_.enableAutoRecovery ? "enabled" : "disabled"));

    return true;
}

void KioskManager::start() {
    if (running_) {
        LOG_WARN("KioskManager already running");
        return;
    }

    LOG_INFO("Starting KioskManager...");
    running_ = true;
    kickWatchdog(); // Initialize watchdog

    monitorThread_ = std::thread(&KioskManager::monitorThreadFunc, this);
}

void KioskManager::stop() {
    if (!running_) {
        return;
    }

    LOG_INFO("Stopping KioskManager...");
    running_ = false;

    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    LOG_INFO("KioskManager stopped");
}

void KioskManager::updateHealth(const HealthMetrics& metrics) {
    std::lock_guard<std::mutex> lock(healthMutex_);
    currentHealth_ = metrics;
}

bool KioskManager::isHealthy() const {
    return systemHealthy_.load();
}

void KioskManager::kickWatchdog() {
    lastWatchdogKick_ = getCurrentTimestamp();
    watchdogExpired_ = false;
}

void KioskManager::reportError(const std::string& errorType, const std::string& message) {
    LOG_ERROR("Error reported: [" << errorType << "] " << message);

    ErrorRecord error;
    error.type = errorType;
    error.message = message;
    error.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(errorsMutex_);
        recentErrors_.push_back(error);

        // Keep only last 100 errors
        if (recentErrors_.size() > 100) {
            recentErrors_.erase(recentErrors_.begin());
        }
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.totalErrors++;
        stats_.lastError = error.timestamp;
    }

    consecutiveErrors_++;
    systemHealthy_ = false;
}

void KioskManager::clearErrors() {
    LOG_INFO("Clearing error state");
    consecutiveErrors_ = 0;
    systemHealthy_ = true;
    watchdogExpired_ = false;
}

void KioskManager::setRestartCallback(RestartCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    restartCallback_ = callback;
}

KioskManager::Statistics KioskManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void KioskManager::monitorThreadFunc() {
    LOG_INFO("Monitor thread started");

    auto lastCheckTime = std::chrono::steady_clock::now();

    while (running_) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - lastCheckTime).count();

        if (elapsed >= config_.healthCheckIntervalSeconds) {
            performHealthCheck();
            lastCheckTime = now;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LOG_INFO("Monitor thread stopped");
}

void KioskManager::performHealthCheck() {
    LOG_DEBUG("Performing health check...");

    // Check watchdog
    if (config_.enableWatchdog) {
        checkWatchdog();
    }

    // Check Kinect health
    checkKinectHealth();

    // Check frame rate
    checkFrameRate();

    // Log status
    logHealthStatus();

    // Attempt recovery if needed
    if (!systemHealthy_ && config_.enableAutoRecovery) {
        int errors = consecutiveErrors_.load();
        if (errors >= config_.maxConsecutiveErrors) {
            LOG_WARN("Too many consecutive errors (" << errors << "), attempting recovery");
            attemptRecovery();
        }
    }
}

void KioskManager::checkWatchdog() {
    uint64_t lastKick = lastWatchdogKick_.load();
    uint64_t now = getCurrentTimestamp();
    uint64_t elapsed = now - lastKick;

    float elapsedSeconds = elapsed / 1000000.0f; // Convert microseconds to seconds

    if (elapsedSeconds > config_.watchdogTimeoutSeconds) {
        if (!watchdogExpired_) {
            LOG_ERROR("Watchdog timeout! System may be hung. Last kick was "
                     << elapsedSeconds << " seconds ago");
            watchdogExpired_ = true;
            reportError("WATCHDOG", "System watchdog timeout");
        }
    }
}

void KioskManager::checkKinectHealth() {
    std::lock_guard<std::mutex> lock(healthMutex_);

    if (!currentHealth_.kinectHealthy) {
        reportError("KINECT", "Kinect device unhealthy");
    }

    if (!currentHealth_.trackerHealthy) {
        reportError("TRACKER", "Body tracker unhealthy");
    }
}

void KioskManager::checkFrameRate() {
    std::lock_guard<std::mutex> lock(healthMutex_);

    float fps = currentHealth_.avgFps.load();
    if (fps < 10.0f && fps > 0.0f) {
        LOG_WARN("Low frame rate detected: " << fps << " FPS");
        reportError("PERFORMANCE", "Low frame rate");
    }
}

void KioskManager::attemptRecovery() {
    LOG_INFO("Attempting auto-recovery...");

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.autoRecoveries++;
    }

    // Wait before recovery
    std::this_thread::sleep_for(
        std::chrono::milliseconds(
            static_cast<int>(config_.autoRestartDelaySeconds * 1000)
        )
    );

    // Call restart callback if set
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (restartCallback_) {
            LOG_INFO("Calling restart callback");
            restartCallback_();

            // Clear errors after recovery attempt
            clearErrors();
        } else {
            LOG_WARN("No restart callback set, cannot auto-recover");
        }
    }
}

uint64_t KioskManager::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void KioskManager::logHealthStatus() const {
    std::lock_guard<std::mutex> lock(healthMutex_);

    LOG_DEBUG("Health Status:");
    LOG_DEBUG("  Kinect: " << (currentHealth_.kinectHealthy ? "OK" : "FAILED"));
    LOG_DEBUG("  Tracker: " << (currentHealth_.trackerHealthy ? "OK" : "FAILED"));
    LOG_DEBUG("  FPS: " << currentHealth_.avgFps.load());
    LOG_DEBUG("  Frames processed: " << currentHealth_.framesProcessed.load());
    LOG_DEBUG("  Frames dropped: " << currentHealth_.framesDropped.load());
    LOG_DEBUG("  Sessions completed: " << currentHealth_.sessionsCompleted.load());
    LOG_DEBUG("  System healthy: " << (systemHealthy_ ? "YES" : "NO"));
}

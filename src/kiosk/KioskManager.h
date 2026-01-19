#pragma once

#include "../../include/common.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

/**
 * KioskManager handles:
 * - System health monitoring
 * - Auto-recovery from errors
 * - Watchdog for hang detection
 * - Periodic maintenance tasks
 * - Session lifecycle management
 */
class KioskManager {
public:
    KioskManager();
    ~KioskManager();

    // Configuration
    struct Config {
        float healthCheckIntervalSeconds = 5.0f;
        float watchdogTimeoutSeconds = 30.0f;
        float autoRestartDelaySeconds = 10.0f;
        int maxConsecutiveErrors = 3;
        bool enableAutoRecovery = true;
        bool enableWatchdog = true;
    };

    // Initialize manager
    bool initialize(const Config& config);

    // Start/stop monitoring
    void start();
    void stop();

    // Health monitoring
    void updateHealth(const HealthMetrics& metrics);
    bool isHealthy() const;

    // Watchdog (call regularly from main loop)
    void kickWatchdog();

    // Error reporting
    void reportError(const std::string& errorType, const std::string& message);
    void clearErrors();

    // Callbacks for recovery actions
    using RestartCallback = std::function<void()>;
    void setRestartCallback(RestartCallback callback);

    // Statistics
    struct Statistics {
        uint64_t totalSessions = 0;
        uint64_t totalErrors = 0;
        uint64_t autoRecoveries = 0;
        std::chrono::system_clock::time_point startTime;
        std::chrono::system_clock::time_point lastError;
    };

    Statistics getStatistics() const;

private:
    // Configuration
    Config config_;

    // Threading
    std::thread monitorThread_;
    std::atomic<bool> running_;

    // Health state
    std::atomic<bool> systemHealthy_;
    std::atomic<int> consecutiveErrors_;
    mutable std::mutex healthMutex_;
    HealthMetrics currentHealth_;

    // Watchdog
    std::atomic<uint64_t> lastWatchdogKick_;
    std::atomic<bool> watchdogExpired_;

    // Statistics
    Statistics stats_;
    mutable std::mutex statsMutex_;

    // Error tracking
    struct ErrorRecord {
        std::string type;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<ErrorRecord> recentErrors_;
    mutable std::mutex errorsMutex_;

    // Callbacks
    RestartCallback restartCallback_;
    std::mutex callbackMutex_;

    // Thread functions
    void monitorThreadFunc();

    // Health checks
    void performHealthCheck();
    void checkWatchdog();
    void checkKinectHealth();
    void checkFrameRate();
    void attemptRecovery();

    // Utility
    uint64_t getCurrentTimestamp() const;
    void logHealthStatus() const;
};

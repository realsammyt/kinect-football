#include "gui/Application.h"
#include "kiosk/KioskManager.h"
#include "kiosk/SessionManager.h"
#include "../include/common.h"
#include <iostream>
#include <csignal>

// Global pointers for signal handling
Application* g_application = nullptr;
KioskManager* g_kioskManager = nullptr;
SessionManager* g_sessionManager = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    LOG_INFO("Signal " << signal << " received, shutting down...");

    if (g_application) {
        g_application->shutdown();
    }

    if (g_kioskManager) {
        g_kioskManager->stop();
    }

    exit(signal);
}

int main(int argc, char** argv) {
    LOG_INFO("========================================");
    LOG_INFO("  FIFA 2026 Soccer Simulator Kiosk");
    LOG_INFO("  Azure Kinect Football Challenge");
    LOG_INFO("========================================");

    // Install signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Create managers
    Application application;
    KioskManager kioskManager;
    SessionManager sessionManager;

    // Set global pointers for signal handler
    g_application = &application;
    g_kioskManager = &kioskManager;
    g_sessionManager = &sessionManager;

    // Initialize Session Manager
    SessionManager::Config sessionConfig;
    sessionConfig.sessionTimeoutSeconds = SESSION_TIMEOUT_SECONDS;
    sessionConfig.playerReidentificationSeconds = 5.0f;
    sessionConfig.maxStoredSessions = 1000;
    sessionConfig.sessionStoragePath = "./sessions";
    sessionConfig.enableAnalytics = true;
    sessionConfig.enableLogging = true;

    if (!sessionManager.initialize(sessionConfig)) {
        LOG_ERROR("Failed to initialize SessionManager");
        return 1;
    }

    // Initialize Kiosk Manager
    KioskManager::Config kioskConfig;
    kioskConfig.healthCheckIntervalSeconds = 5.0f;
    kioskConfig.watchdogTimeoutSeconds = 30.0f;
    kioskConfig.autoRestartDelaySeconds = 10.0f;
    kioskConfig.maxConsecutiveErrors = 3;
    kioskConfig.enableAutoRecovery = true;
    kioskConfig.enableWatchdog = true;

    if (!kioskManager.initialize(kioskConfig)) {
        LOG_ERROR("Failed to initialize KioskManager");
        return 1;
    }

    // Set up restart callback
    kioskManager.setRestartCallback([&application]() {
        LOG_INFO("KioskManager requested restart");
        application.shutdown();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (application.initialize()) {
            // Application will be restarted by main loop
            LOG_INFO("Application reinitialized successfully");
        } else {
            LOG_ERROR("Failed to reinitialize application");
        }
    });

    // Set up session timeout callback
    sessionManager.setTimeoutCallback([&sessionManager](const std::string& sessionId) {
        LOG_INFO("Session timeout callback for: " << sessionId);
        sessionManager.cancelSession(sessionId);
    });

    // Initialize Application
    if (!application.initialize()) {
        LOG_ERROR("Failed to initialize Application");
        return 1;
    }

    // Start Kiosk Manager
    kioskManager.start();

    // Main loop - this blocks until shutdown
    try {
        LOG_INFO("Starting main application loop...");
        application.run();

    } catch (const std::exception& e) {
        LOG_ERROR("Exception in main loop: " << e.what());
        kioskManager.reportError("MAIN_LOOP", e.what());
    }

    // Cleanup
    LOG_INFO("Cleaning up...");
    kioskManager.stop();

    // Export final session data
    LOG_INFO("Exporting session data...");
    sessionManager.exportSessions("./sessions/export_final.csv");

    // Print final statistics
    auto kioskStats = kioskManager.getStatistics();
    auto sessionAnalytics = sessionManager.getAnalytics();

    LOG_INFO("========================================");
    LOG_INFO("  FINAL STATISTICS");
    LOG_INFO("========================================");
    LOG_INFO("Kiosk Statistics:");
    LOG_INFO("  Total sessions: " << kioskStats.totalSessions);
    LOG_INFO("  Total errors: " << kioskStats.totalErrors);
    LOG_INFO("  Auto recoveries: " << kioskStats.autoRecoveries);

    LOG_INFO("Session Analytics:");
    LOG_INFO("  Total sessions: " << sessionAnalytics.totalSessions);
    LOG_INFO("  Completed: " << sessionAnalytics.completedSessions);
    LOG_INFO("  Cancelled: " << sessionAnalytics.cancelledSessions);
    LOG_INFO("  Shared: " << sessionAnalytics.sharedSessions);
    LOG_INFO("  Avg duration: " << sessionAnalytics.avgSessionDurationSeconds << "s");
    LOG_INFO("  Avg score: " << sessionAnalytics.avgScore << "%");
    LOG_INFO("========================================");

    // Clear global pointers
    g_application = nullptr;
    g_kioskManager = nullptr;
    g_sessionManager = nullptr;

    LOG_INFO("Shutdown complete. Goodbye!");
    return 0;
}

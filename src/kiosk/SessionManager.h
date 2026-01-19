#pragma once

#include "../../include/common.h"
#include <unordered_map>
#include <deque>
#include <mutex>
#include <functional>

/**
 * SessionManager handles:
 * - Player session lifecycle
 * - Session data storage and retrieval
 * - Player identification and tracking
 * - Session timeout management
 * - Analytics and logging
 */
class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Configuration
    struct Config {
        float sessionTimeoutSeconds = 60.0f;
        float playerReidentificationSeconds = 5.0f;
        size_t maxStoredSessions = 1000;
        std::string sessionStoragePath = "./sessions";
        bool enableAnalytics = true;
        bool enableLogging = true;
    };

    // Initialize manager
    bool initialize(const Config& config);

    // Session lifecycle
    std::string startSession(uint32_t playerId);
    void endSession(const std::string& sessionId, const ChallengeResult& result);
    void cancelSession(const std::string& sessionId);

    // Session queries
    SessionData* getSession(const std::string& sessionId);
    SessionData* getActiveSession();
    bool hasActiveSession() const;

    // Player tracking
    void updatePlayerPresence(uint32_t playerId);
    bool isPlayerActive(uint32_t playerId) const;
    uint32_t getActivePlayerId() const;

    // Session data
    void setChallenge(const std::string& sessionId, ChallengeType challenge);
    void setShareData(const std::string& sessionId,
                      const std::string& method,
                      const std::string& url);

    // Analytics
    struct Analytics {
        uint64_t totalSessions = 0;
        uint64_t completedSessions = 0;
        uint64_t cancelledSessions = 0;
        uint64_t sharedSessions = 0;

        std::map<ChallengeType, uint64_t> challengeCounts;
        std::map<std::string, uint64_t> shareMethodCounts;

        float avgSessionDurationSeconds = 0.0f;
        float avgScore = 0.0f;

        std::chrono::system_clock::time_point firstSession;
        std::chrono::system_clock::time_point lastSession;
    };

    Analytics getAnalytics() const;
    void resetAnalytics();

    // Session history
    std::vector<SessionData> getRecentSessions(size_t count) const;
    void exportSessions(const std::string& filepath) const;

    // Timeout management
    void checkTimeouts();
    using TimeoutCallback = std::function<void(const std::string& sessionId)>;
    void setTimeoutCallback(TimeoutCallback callback);

private:
    // Configuration
    Config config_;

    // Active session
    std::string activeSessionId_;
    uint32_t activePlayerId_;
    std::chrono::steady_clock::time_point lastPlayerUpdate_;
    mutable std::mutex activeMutex_;

    // Session storage
    std::unordered_map<std::string, SessionData> sessions_;
    std::deque<std::string> sessionHistory_; // Ordered by time
    mutable std::mutex sessionsMutex_;

    // Analytics
    Analytics analytics_;
    mutable std::mutex analyticsMutex_;

    // Callbacks
    TimeoutCallback timeoutCallback_;
    std::mutex callbackMutex_;

    // Internal helpers
    void updateAnalytics(const SessionData& session);
    void saveSession(const SessionData& session);
    void pruneOldSessions();
    bool isSessionTimedOut(const SessionData& session) const;

    // Logging
    void logSessionStart(const SessionData& session);
    void logSessionEnd(const SessionData& session);
};

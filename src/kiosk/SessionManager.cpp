#include "SessionManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace kinect {
namespace kiosk {

SessionManager::SessionManager()
    : activePlayerId_(0)
{
    analytics_.firstSession = std::chrono::system_clock::now();
}

SessionManager::~SessionManager() {
}

bool SessionManager::initialize(const Config& config) {
    config_ = config;

    LOG_INFO("SessionManager initialized");
    LOG_INFO("  Session timeout: " << config_.sessionTimeoutSeconds << "s");
    LOG_INFO("  Storage path: " << config_.sessionStoragePath);
    LOG_INFO("  Analytics: " << (config_.enableAnalytics ? "enabled" : "disabled"));

    // Create storage directory if it doesn't exist
    if (!config_.sessionStoragePath.empty()) {
        try {
            fs::create_directories(config_.sessionStoragePath);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create session storage directory: " << e.what());
            return false;
        }
    }

    return true;
}

std::string SessionManager::startSession(uint32_t playerId) {
    std::scoped_lock lock(activeMutex_, sessionsMutex_);

    // End any existing active session
    if (!activeSessionId_.empty()) {
        LOG_WARN("Starting new session while one is active, ending previous");
        // Note: Don't call endSession here as it would deadlock
    }

    // Create new session
    SessionData session;
    session.sessionId = util::generateSessionId();
    session.playerId = playerId;
    session.startTime = std::chrono::system_clock::now();

    // Store session
    sessions_[session.sessionId] = session;
    sessionHistory_.push_back(session.sessionId);

    // Set as active
    activeSessionId_ = session.sessionId;
    activePlayerId_ = playerId;
    lastPlayerUpdate_ = std::chrono::steady_clock::now();

    // Update analytics
    {
        std::lock_guard<std::mutex> analyticsLock(analyticsMutex_);
        analytics_.totalSessions++;
        analytics_.lastSession = session.startTime;
    }

    logSessionStart(session);

    LOG_INFO("Session started: " << session.sessionId << " for player " << playerId);

    return session.sessionId;
}

void SessionManager::endSession(const std::string& sessionId, const ChallengeResult& result) {
    std::scoped_lock lock(activeMutex_, sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOG_WARN("Attempted to end non-existent session: " << sessionId);
        return;
    }

    SessionData& session = it->second;
    session.endTime = std::chrono::system_clock::now();
    session.result = result;

    // Update analytics
    updateAnalytics(session);

    // Save to disk
    if (config_.enableLogging) {
        saveSession(session);
    }

    // Clear active session
    if (activeSessionId_ == sessionId) {
        activeSessionId_.clear();
        activePlayerId_ = 0;
    }

    logSessionEnd(session);

    LOG_INFO("Session ended: " << sessionId);

    // Prune old sessions
    pruneOldSessions();
}

void SessionManager::cancelSession(const std::string& sessionId) {
    std::scoped_lock lock(activeMutex_, sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        LOG_WARN("Attempted to cancel non-existent session: " << sessionId);
        return;
    }

    SessionData& session = it->second;
    session.endTime = std::chrono::system_clock::now();

    // Update analytics
    {
        std::lock_guard<std::mutex> analyticsLock(analyticsMutex_);
        analytics_.cancelledSessions++;
    }

    // Clear active session
    if (activeSessionId_ == sessionId) {
        activeSessionId_.clear();
        activePlayerId_ = 0;
    }

    LOG_INFO("Session cancelled: " << sessionId);
}

SessionData* SessionManager::getSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

SessionData* SessionManager::getActiveSession() {
    std::scoped_lock lock(activeMutex_, sessionsMutex_);

    if (activeSessionId_.empty()) {
        return nullptr;
    }

    auto it = sessions_.find(activeSessionId_);
    if (it != sessions_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SessionManager::hasActiveSession() const {
    std::lock_guard<std::mutex> lock(activeMutex_);
    return !activeSessionId_.empty();
}

void SessionManager::updatePlayerPresence(uint32_t playerId) {
    std::lock_guard<std::mutex> lock(activeMutex_);

    if (activePlayerId_ == playerId) {
        lastPlayerUpdate_ = std::chrono::steady_clock::now();
    }
}

bool SessionManager::isPlayerActive(uint32_t playerId) const {
    std::lock_guard<std::mutex> lock(activeMutex_);

    if (activePlayerId_ != playerId) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - lastPlayerUpdate_).count();

    return elapsed < config_.playerReidentificationSeconds;
}

uint32_t SessionManager::getActivePlayerId() const {
    std::lock_guard<std::mutex> lock(activeMutex_);
    return activePlayerId_;
}

void SessionManager::setChallenge(const std::string& sessionId, ChallengeType challenge) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.selectedChallenge = challenge;
        LOG_INFO("Challenge set for session " << sessionId << ": " << static_cast<int>(challenge));
    }
}

void SessionManager::setShareData(const std::string& sessionId,
                                  const std::string& method,
                                  const std::string& url) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.wasShared = true;
        it->second.shareMethod = method;
        it->second.downloadUrl = url;

        // Update analytics
        {
            std::lock_guard<std::mutex> analyticsLock(analyticsMutex_);
            analytics_.sharedSessions++;
            analytics_.shareMethodCounts[method]++;
        }

        LOG_INFO("Share data set for session " << sessionId << ": " << method);
    }
}

SessionManager::Analytics SessionManager::getAnalytics() const {
    std::lock_guard<std::mutex> lock(analyticsMutex_);
    return analytics_;
}

void SessionManager::resetAnalytics() {
    std::lock_guard<std::mutex> lock(analyticsMutex_);

    analytics_ = Analytics();
    analytics_.firstSession = std::chrono::system_clock::now();

    LOG_INFO("Analytics reset");
}

std::vector<SessionData> SessionManager::getRecentSessions(size_t count) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    std::vector<SessionData> recent;
    recent.reserve(std::min(count, sessionHistory_.size()));

    // Iterate from end (most recent)
    auto it = sessionHistory_.rbegin();
    size_t added = 0;

    while (it != sessionHistory_.rend() && added < count) {
        auto sessionIt = sessions_.find(*it);
        if (sessionIt != sessions_.end()) {
            recent.push_back(sessionIt->second);
            added++;
        }
        ++it;
    }

    return recent;
}

void SessionManager::exportSessions(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    try {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for export: " << filepath);
            return;
        }

        // Write header
        file << "SessionID,PlayerID,StartTime,EndTime,Challenge,Score,Accuracy,Shared,ShareMethod\n";

        // Write sessions
        for (const auto& sessionId : sessionHistory_) {
            auto it = sessions_.find(sessionId);
            if (it == sessions_.end()) continue;

            const SessionData& session = it->second;

            file << session.sessionId << ","
                 << session.playerId << ","
                 << std::chrono::system_clock::to_time_t(session.startTime) << ","
                 << std::chrono::system_clock::to_time_t(session.endTime) << ","
                 << static_cast<int>(session.selectedChallenge) << ","
                 << session.result.score << ","
                 << session.result.accuracy << ","
                 << (session.wasShared ? "1" : "0") << ","
                 << session.shareMethod << "\n";
        }

        file.close();
        LOG_INFO("Exported " << sessionHistory_.size() << " sessions to " << filepath);

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to export sessions: " << e.what());
    }
}

void SessionManager::checkTimeouts() {
    std::scoped_lock lock(activeMutex_, sessionsMutex_);

    if (activeSessionId_.empty()) {
        return;
    }

    auto it = sessions_.find(activeSessionId_);
    if (it == sessions_.end()) {
        return;
    }

    if (isSessionTimedOut(it->second)) {
        LOG_INFO("Session timeout detected: " << activeSessionId_);

        // Call timeout callback
        std::string sessionId = activeSessionId_;
        {
            std::lock_guard<std::mutex> callbackLock(callbackMutex_);
            if (timeoutCallback_) {
                timeoutCallback_(sessionId);
            }
        }
    }
}

void SessionManager::setTimeoutCallback(TimeoutCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    timeoutCallback_ = callback;
}

void SessionManager::updateAnalytics(const SessionData& session) {
    std::lock_guard<std::mutex> lock(analyticsMutex_);

    analytics_.completedSessions++;
    analytics_.challengeCounts[session.selectedChallenge]++;

    // Update averages
    uint64_t total = analytics_.completedSessions;
    if (total > 0) {
        // Running average for duration
        float sessionDuration = session.getDurationMs() / 1000.0f;
        analytics_.avgSessionDurationSeconds =
            ((analytics_.avgSessionDurationSeconds * (total - 1)) + sessionDuration) / total;

        // Running average for score
        float score = session.result.getPercentage();
        analytics_.avgScore =
            ((analytics_.avgScore * (total - 1)) + score) / total;
    }
}

void SessionManager::saveSession(const SessionData& session) {
    if (config_.sessionStoragePath.empty()) {
        return;
    }

    try {
        // Create filename with timestamp
        auto time_t = std::chrono::system_clock::to_time_t(session.startTime);
        std::tm tm;
        localtime_s(&tm, &time_t);

        char filename[64];
        std::strftime(filename, sizeof(filename), "%Y%m%d_%H%M%S", &tm);

        std::string filepath = config_.sessionStoragePath + "/" +
                              std::string(filename) + "_" +
                              session.sessionId + ".json";

        std::ofstream file(filepath);
        if (!file.is_open()) {
            LOG_ERROR("Failed to save session to file: " << filepath);
            return;
        }

        // Write session data as JSON (simplified)
        file << "{\n";
        file << "  \"sessionId\": \"" << session.sessionId << "\",\n";
        file << "  \"playerId\": " << session.playerId << ",\n";
        file << "  \"challenge\": " << static_cast<int>(session.selectedChallenge) << ",\n";
        file << "  \"score\": " << session.result.score << ",\n";
        file << "  \"accuracy\": " << session.result.accuracy << ",\n";
        file << "  \"duration_ms\": " << session.getDurationMs() << ",\n";
        file << "  \"shared\": " << (session.wasShared ? "true" : "false") << "\n";
        file << "}\n";

        file.close();

    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save session: " << e.what());
    }
}

void SessionManager::pruneOldSessions() {
    if (sessionHistory_.size() <= config_.maxStoredSessions) {
        return;
    }

    size_t toRemove = sessionHistory_.size() - config_.maxStoredSessions;
    LOG_INFO("Pruning " << toRemove << " old sessions");

    for (size_t i = 0; i < toRemove; ++i) {
        std::string sessionId = sessionHistory_.front();
        sessionHistory_.pop_front();
        sessions_.erase(sessionId);
    }
}

bool SessionManager::isSessionTimedOut(const SessionData& session) const {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - session.startTime
    ).count();

    return elapsed >= config_.sessionTimeoutSeconds;
}

void SessionManager::logSessionStart(const SessionData& session) {
    if (!config_.enableLogging) return;

    LOG_INFO("=== SESSION START ===");
    LOG_INFO("  ID: " << session.sessionId);
    LOG_INFO("  Player: " << session.playerId);
    LOG_INFO("  Time: " << std::chrono::system_clock::to_time_t(session.startTime));
}

void SessionManager::logSessionEnd(const SessionData& session) {
    if (!config_.enableLogging) return;

    LOG_INFO("=== SESSION END ===");
    LOG_INFO("  ID: " << session.sessionId);
    LOG_INFO("  Duration: " << session.getDurationMs() / 1000.0f << "s");
    LOG_INFO("  Challenge: " << static_cast<int>(session.selectedChallenge));
    LOG_INFO("  Score: " << session.result.score << "/" << session.result.maxScore);
    LOG_INFO("  Accuracy: " << session.result.accuracy << "%");
    LOG_INFO("  Shared: " << (session.wasShared ? "Yes" : "No"));
}

} // namespace kiosk
} // namespace kinect

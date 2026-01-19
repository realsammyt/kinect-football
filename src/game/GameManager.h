#pragma once

#include "ChallengeBase.h"
#include "../../include/GameConfig.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>

namespace kinect {
namespace game {

// Forward declarations
class AccuracyChallenge;
class PowerChallenge;
class PenaltyShootout;

// Session stats
struct SessionStats {
    int32_t totalScore = 0;
    int32_t challengesCompleted = 0;
    int32_t totalKicks = 0;
    float avgAccuracy = 0.0f;
    float maxVelocity = 0.0f;
    float sessionDuration = 0.0f;
    std::vector<std::string> achievementsUnlocked;
    std::map<ChallengeType, int32_t> bestScores;
};

// Event callbacks
using ChallengeStartCallback = std::function<void(ChallengeType)>;
using ChallengeCompleteCallback = std::function<void(const ChallengeResult&)>;
using AchievementUnlockedCallback = std::function<void(const AchievementConfig&)>;

class GameManager {
public:
    explicit GameManager(const GameConfig& config = GameConfig());
    ~GameManager();

    // Lifecycle
    void initialize();
    void shutdown();

    // Challenge management
    bool startChallenge(ChallengeType type);
    void stopCurrentChallenge();
    void pauseCurrentChallenge();
    void resumeCurrentChallenge();

    // Frame processing
    void processFrame(const k4abt_skeleton_t& skeleton,
                     const k4a_image_t& depthImage,
                     float deltaTime);

    // Rendering
    void render(cv::Mat& frame);

    // State queries
    bool hasActiveChallenge() const;
    ChallengeType getCurrentChallengeType() const;
    ChallengeState getCurrentChallengeState() const;
    ChallengeBase* getCurrentChallenge() const { return currentChallenge_.get(); }

    // Session management
    void startSession();
    void endSession();
    bool isSessionActive() const { return sessionActive_; }
    const SessionStats& getSessionStats() const { return sessionStats_; }

    // Configuration
    const GameConfig& getConfig() const { return config_; }
    void updateConfig(const GameConfig& config) { config_ = config; }

    // Achievements
    void checkAchievements(const ChallengeResult& result);
    bool isAchievementUnlocked(const std::string& achievementId) const;
    std::vector<AchievementConfig> getUnlockedAchievements() const;
    std::vector<AchievementConfig> getAllAchievements() const;

    // Event callbacks
    void setOnChallengeStart(ChallengeStartCallback callback) {
        onChallengeStart_ = callback;
    }
    void setOnChallengeComplete(ChallengeCompleteCallback callback) {
        onChallengeComplete_ = callback;
    }
    void setOnAchievementUnlocked(AchievementUnlockedCallback callback) {
        onAchievementUnlocked_ = callback;
    }

private:
    // Challenge factory
    std::unique_ptr<ChallengeBase> createChallenge(ChallengeType type);

    // Session tracking
    void updateSessionStats(const ChallengeResult& result);

    // Achievement checking
    void checkAccuracyAchievements(const ChallengeResult& result);
    void checkPowerAchievements(const ChallengeResult& result);
    void checkPenaltyAchievements(const ChallengeResult& result);
    void unlockAchievement(const std::string& achievementId);

    // Members
    GameConfig config_;
    std::unique_ptr<ChallengeBase> currentChallenge_;
    bool sessionActive_;
    SessionStats sessionStats_;

    // Session timing
    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint sessionStartTime_;

    // Event callbacks
    ChallengeStartCallback onChallengeStart_;
    ChallengeCompleteCallback onChallengeComplete_;
    AchievementUnlockedCallback onAchievementUnlocked_;

    // Achievement tracking (lifetime stats)
    struct LifetimeStats {
        int32_t totalPenaltiesScored = 0;
        std::vector<float> recentPowerKicks;  // Last 3 kicks
        bool suddenDeathWon = false;
    } lifetimeStats_;
};

} // namespace game
} // namespace kinect

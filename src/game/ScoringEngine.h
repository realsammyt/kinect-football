#pragma once

#include "ChallengeBase.h"
#include "../../include/GameConfig.h"
#include <vector>
#include <map>

namespace kinect {
namespace game {

// Combo tracking
struct ComboTracker {
    int32_t currentStreak = 0;
    int32_t maxStreak = 0;
    float lastSuccessTime = 0.0f;
    float comboTimeWindow = 3.0f;  // seconds between successes

    void recordSuccess(float currentTime);
    void reset();
    bool isActive() const { return currentStreak > 0; }
};

// Score breakdown
struct ScoreBreakdown {
    int32_t baseScore = 0;
    int32_t accuracyBonus = 0;
    int32_t powerBonus = 0;
    int32_t techniqueBonus = 0;
    int32_t comboBonus = 0;
    int32_t timeBonus = 0;
    int32_t completionBonus = 0;

    int32_t getTotalScore() const {
        return baseScore + accuracyBonus + powerBonus + techniqueBonus +
               comboBonus + timeBonus + completionBonus;
    }
};

class ScoringEngine {
public:
    explicit ScoringEngine(const ScoringConfig& config);

    // Score calculation
    int32_t calculateKickScore(float accuracy, float power, float technique);
    int32_t calculateAccuracyScore(int32_t hits, int32_t attempts,
                                   const std::vector<TargetZone>& zones);
    int32_t calculatePowerScore(float velocityKmh);
    int32_t calculatePenaltyScore(int32_t goals, int32_t attempts);

    // Combo system
    void recordSuccess(float currentTime);
    void resetCombo();
    int32_t getCurrentStreak() const { return combo_.currentStreak; }
    int32_t getMaxStreak() const { return combo_.maxStreak; }
    float getComboMultiplier() const;

    // Time bonuses
    int32_t calculateTimeBonus(float timeRemaining);

    // Achievements
    bool checkAchievement(const AchievementConfig& config,
                         const ChallengeResult& result);
    std::vector<std::string> checkAllAchievements(
        const std::vector<AchievementConfig>& achievements,
        const ChallengeResult& result);

    // Score breakdown
    ScoreBreakdown getBreakdown() const { return breakdown_; }
    void resetBreakdown();

    // Configuration
    void updateConfig(const ScoringConfig& config) { config_ = config; }
    const ScoringConfig& getConfig() const { return config_; }

private:
    ScoringConfig config_;
    ComboTracker combo_;
    ScoreBreakdown breakdown_;
};

// Achievement checker utilities
class AchievementChecker {
public:
    // Accuracy achievements
    static bool checkBullseye(const ChallengeResult& result,
                             const std::vector<TargetZone>& zones);
    static bool checkCornerSpecialist(const std::vector<TargetZone>& zones);
    static bool checkSharpshooter(const ChallengeResult& result);

    // Power achievements
    static bool checkThunderstrike(float velocityKmh);
    static bool checkRocketShot(float velocityKmh);
    static bool checkConsistentPower(const std::vector<float>& recentKicks);

    // Penalty achievements
    static bool checkPerfectFive(int32_t goals, int32_t attempts);
    static bool checkIceCold(bool suddenDeathWon);
    static bool checkPenaltyMaster(int32_t lifetimeGoals);
};

// Leaderboard entry
struct LeaderboardEntry {
    std::string playerName;
    int32_t score;
    float accuracy;
    float maxVelocity;
    uint64_t timestamp;
    std::string grade;

    bool operator<(const LeaderboardEntry& other) const {
        return score > other.score;  // Higher scores first
    }
};

class Leaderboard {
public:
    explicit Leaderboard(size_t maxEntries = 10);

    // Entry management
    bool addEntry(const LeaderboardEntry& entry);
    std::vector<LeaderboardEntry> getTopEntries(size_t count = 10) const;
    std::vector<LeaderboardEntry> getEntriesForChallenge(ChallengeType type) const;

    // Ranking
    int32_t getRank(int32_t score) const;
    bool isHighScore(int32_t score) const;

    // Persistence
    bool save(const std::string& filepath);
    bool load(const std::string& filepath);

    void clear() { entries_.clear(); }
    size_t getEntryCount() const { return entries_.size(); }

private:
    std::map<ChallengeType, std::vector<LeaderboardEntry>> entries_;
    size_t maxEntries_;
};

} // namespace game
} // namespace kinect

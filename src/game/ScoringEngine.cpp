#include "ScoringEngine.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace kinect {
namespace game {

// ComboTracker implementation
void ComboTracker::recordSuccess(float currentTime) {
    if (lastSuccessTime > 0 && (currentTime - lastSuccessTime) <= comboTimeWindow) {
        currentStreak++;
    } else {
        currentStreak = 1;
    }

    maxStreak = std::max(maxStreak, currentStreak);
    lastSuccessTime = currentTime;
}

void ComboTracker::reset() {
    currentStreak = 0;
    maxStreak = 0;
    lastSuccessTime = 0.0f;
}

// ScoringEngine implementation
ScoringEngine::ScoringEngine(const ScoringConfig& config)
    : config_(config)
{
}

int32_t ScoringEngine::calculateKickScore(float accuracy, float power, float technique) {
    breakdown_.baseScore = config_.basePoints;

    // Apply multipliers
    if (accuracy > 0.7f) {
        breakdown_.accuracyBonus = static_cast<int32_t>(
            config_.basePoints * config_.accuracyMultiplier
        );
    }

    if (power > 80.0f) {  // km/h
        breakdown_.powerBonus = static_cast<int32_t>(
            config_.basePoints * config_.powerMultiplier
        );
    }

    if (technique > 1.2f) {
        breakdown_.techniqueBonus = static_cast<int32_t>(
            config_.basePoints * config_.techniqueMultiplier
        );
    }

    // Combo bonus
    if (combo_.currentStreak >= config_.streakThreshold) {
        breakdown_.comboBonus = static_cast<int32_t>(
            config_.basePoints * getComboMultiplier()
        );
    }

    return breakdown_.getTotalScore();
}

int32_t ScoringEngine::calculateAccuracyScore(int32_t hits, int32_t attempts,
                                               const std::vector<TargetZone>& zones)
{
    breakdown_.baseScore = hits * config_.basePoints;

    // Accuracy bonus
    float accuracy = attempts > 0 ? static_cast<float>(hits) / attempts : 0.0f;
    if (accuracy >= 0.8f) {
        breakdown_.accuracyBonus = static_cast<int32_t>(
            breakdown_.baseScore * config_.accuracyMultiplier
        );
    }

    // Zone multipliers
    int32_t zoneBonus = 0;
    for (const auto& zone : zones) {
        if (zone.isHit) {
            zoneBonus += static_cast<int32_t>(
                config_.basePoints * (zone.scoreMultiplier - 1.0f)
            );
        }
    }
    breakdown_.accuracyBonus += zoneBonus;

    return breakdown_.getTotalScore();
}

int32_t ScoringEngine::calculatePowerScore(float velocityKmh) {
    breakdown_.baseScore = static_cast<int32_t>(velocityKmh * 10);

    if (velocityKmh >= 100.0f) {
        breakdown_.powerBonus = 1500;
    } else if (velocityKmh >= 80.0f) {
        breakdown_.powerBonus = 500;
    }

    return breakdown_.getTotalScore();
}

int32_t ScoringEngine::calculatePenaltyScore(int32_t goals, int32_t attempts) {
    breakdown_.baseScore = goals * 200;

    // Perfect score bonus
    if (goals == 5 && attempts == 5) {
        breakdown_.completionBonus = 1000;
    }

    return breakdown_.getTotalScore();
}

void ScoringEngine::recordSuccess(float currentTime) {
    combo_.recordSuccess(currentTime);
}

void ScoringEngine::resetCombo() {
    combo_.reset();
}

float ScoringEngine::getComboMultiplier() const {
    if (combo_.currentStreak < config_.streakThreshold) {
        return 1.0f;
    }

    int32_t streakBonus = combo_.currentStreak - config_.streakThreshold;
    return 1.0f + config_.streakBonusPerKick * streakBonus;
}

int32_t ScoringEngine::calculateTimeBonus(float timeRemaining) {
    if (!config_.hasTimeBonus) {
        return 0;
    }

    breakdown_.timeBonus = static_cast<int32_t>(
        timeRemaining * config_.timeBonusPerSecond
    );

    return breakdown_.timeBonus;
}

bool ScoringEngine::checkAchievement(const AchievementConfig& config,
                                     const ChallengeResult& result)
{
    // Score requirement
    if (config.requiredScore > 0 && result.finalScore < config.requiredScore) {
        return false;
    }

    // Attempts requirement
    if (config.requiredAttempts > 0 && result.attempts < config.requiredAttempts) {
        return false;
    }

    // Accuracy requirement
    if (config.requiredAccuracy > 0.0f && result.accuracy < config.requiredAccuracy) {
        return false;
    }

    // Velocity requirement
    if (config.requiredVelocity > 0.0f && result.maxVelocity < config.requiredVelocity) {
        return false;
    }

    return true;
}

std::vector<std::string> ScoringEngine::checkAllAchievements(
    const std::vector<AchievementConfig>& achievements,
    const ChallengeResult& result)
{
    std::vector<std::string> unlockedIds;

    for (const auto& achievement : achievements) {
        if (achievement.challengeType == result.type &&
            !achievement.isUnlocked &&
            checkAchievement(achievement, result))
        {
            unlockedIds.push_back(achievement.id);
        }
    }

    return unlockedIds;
}

void ScoringEngine::resetBreakdown() {
    breakdown_ = ScoreBreakdown();
}

// AchievementChecker implementation
bool AchievementChecker::checkBullseye(const ChallengeResult& result,
                                       const std::vector<TargetZone>& zones)
{
    for (const auto& zone : zones) {
        if (!zone.isHit) {
            return false;
        }
    }
    return true;
}

bool AchievementChecker::checkCornerSpecialist(const std::vector<TargetZone>& zones) {
    // Check all 4 corners
    std::vector<TargetZone::Position> corners = {
        TargetZone::Position::TOP_LEFT,
        TargetZone::Position::TOP_RIGHT,
        TargetZone::Position::BOTTOM_LEFT,
        TargetZone::Position::BOTTOM_RIGHT
    };

    for (auto corner : corners) {
        bool found = false;
        for (const auto& zone : zones) {
            if (zone.position == corner && zone.isHit) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

bool AchievementChecker::checkSharpshooter(const ChallengeResult& result) {
    return result.attempts >= 10 && result.accuracy >= 0.8f;
}

bool AchievementChecker::checkThunderstrike(float velocityKmh) {
    return velocityKmh >= 100.0f;
}

bool AchievementChecker::checkRocketShot(float velocityKmh) {
    return velocityKmh >= 120.0f;
}

bool AchievementChecker::checkConsistentPower(const std::vector<float>& recentKicks) {
    if (recentKicks.size() < 3) {
        return false;
    }

    // Check last 3 kicks
    for (size_t i = recentKicks.size() - 3; i < recentKicks.size(); i++) {
        if (recentKicks[i] < 80.0f) {
            return false;
        }
    }

    return true;
}

bool AchievementChecker::checkPerfectFive(int32_t goals, int32_t attempts) {
    return goals == 5 && attempts == 5;
}

bool AchievementChecker::checkIceCold(bool suddenDeathWon) {
    return suddenDeathWon;
}

bool AchievementChecker::checkPenaltyMaster(int32_t lifetimeGoals) {
    return lifetimeGoals >= 20;
}

// Leaderboard implementation
Leaderboard::Leaderboard(size_t maxEntries)
    : maxEntries_(maxEntries)
{
}

bool Leaderboard::addEntry(const LeaderboardEntry& entry) {
    // This is a simplified version - would need to store challenge type with entry
    ChallengeType type = ChallengeType::ACCURACY;  // Default
    auto& entries = entries_[type];

    entries.push_back(entry);
    std::sort(entries.begin(), entries.end());

    // Keep only top N entries
    if (entries.size() > maxEntries_) {
        entries.resize(maxEntries_);
    }

    return true;
}

std::vector<LeaderboardEntry> Leaderboard::getTopEntries(size_t count) const {
    std::vector<LeaderboardEntry> allEntries;

    for (const auto& pair : entries_) {
        allEntries.insert(allEntries.end(), pair.second.begin(), pair.second.end());
    }

    std::sort(allEntries.begin(), allEntries.end());

    if (allEntries.size() > count) {
        allEntries.resize(count);
    }

    return allEntries;
}

std::vector<LeaderboardEntry> Leaderboard::getEntriesForChallenge(ChallengeType type) const {
    auto it = entries_.find(type);
    if (it != entries_.end()) {
        return it->second;
    }
    return {};
}

int32_t Leaderboard::getRank(int32_t score) const {
    auto allEntries = getTopEntries(1000);
    int32_t rank = 1;

    for (const auto& entry : allEntries) {
        if (score > entry.score) {
            return rank;
        }
        rank++;
    }

    return rank;
}

bool Leaderboard::isHighScore(int32_t score) const {
    auto allEntries = getTopEntries(maxEntries_);

    if (allEntries.size() < maxEntries_) {
        return true;  // Not full yet
    }

    return score > allEntries.back().score;
}

bool Leaderboard::save(const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : entries_) {
        ChallengeType type = pair.first;
        for (const auto& entry : pair.second) {
            file << static_cast<int>(type) << ","
                 << entry.playerName << ","
                 << entry.score << ","
                 << entry.accuracy << ","
                 << entry.maxVelocity << ","
                 << entry.timestamp << ","
                 << entry.grade << "\n";
        }
    }

    file.close();
    return true;
}

bool Leaderboard::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    entries_.clear();

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string field;

        LeaderboardEntry entry;
        int typeInt;

        std::getline(ss, field, ',');
        typeInt = std::stoi(field);

        std::getline(ss, entry.playerName, ',');

        std::getline(ss, field, ',');
        entry.score = std::stoi(field);

        std::getline(ss, field, ',');
        entry.accuracy = std::stof(field);

        std::getline(ss, field, ',');
        entry.maxVelocity = std::stof(field);

        std::getline(ss, field, ',');
        entry.timestamp = std::stoull(field);

        std::getline(ss, entry.grade, ',');

        entries_[static_cast<ChallengeType>(typeInt)].push_back(entry);
    }

    file.close();

    // Sort all entries
    for (auto& pair : entries_) {
        std::sort(pair.second.begin(), pair.second.end());
    }

    return true;
}

} // namespace game
} // namespace kinect

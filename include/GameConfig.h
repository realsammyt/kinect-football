#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kinect {
namespace game {

// Challenge types
enum class ChallengeType {
    ACCURACY,          // Target zone shooting
    POWER,             // Maximum kick power
    PENALTY_SHOOTOUT,  // Classic penalties vs goalkeeper
    FREE_KICK,         // Curve and accuracy
    SKILL_MOVE         // Technique combos
};

// Challenge state
enum class ChallengeState {
    IDLE,          // Not started
    INSTRUCTIONS,  // Showing instructions
    COUNTDOWN,     // 3-2-1 countdown
    ACTIVE,        // Challenge in progress
    PAUSED,        // Temporarily paused
    COMPLETE       // Challenge finished
};

// Scoring configuration
struct ScoringConfig {
    int32_t basePoints = 100;

    // Multipliers
    float accuracyMultiplier = 1.5f;   // Hitting target zones
    float powerMultiplier = 1.2f;      // High velocity kicks
    float techniqueMultiplier = 2.0f;  // Proper form
    float comboMultiplier = 1.5f;      // Consecutive successes

    // Streak bonuses
    int32_t streakThreshold = 3;       // Kicks to start streak
    float streakBonusPerKick = 0.25f;  // Additional multiplier per streak kick

    // Time bonuses
    bool hasTimeBonus = false;
    float timeBonusPerSecond = 10.0f;
};

// Target zone in 3x3 grid
struct TargetZone {
    enum class Position {
        TOP_LEFT = 0,    TOP_CENTER = 1,    TOP_RIGHT = 2,
        MID_LEFT = 3,    MID_CENTER = 4,    MID_RIGHT = 5,
        BOTTOM_LEFT = 6, BOTTOM_CENTER = 7, BOTTOM_RIGHT = 8
    };

    Position position;
    float scoreMultiplier;  // Corner = 3x, Edge = 2x, Center = 1x
    bool isHit = false;

    static float getMultiplierForPosition(Position pos) {
        // Corners: 3x
        if (pos == Position::TOP_LEFT || pos == Position::TOP_RIGHT ||
            pos == Position::BOTTOM_LEFT || pos == Position::BOTTOM_RIGHT) {
            return 3.0f;
        }
        // Edges: 2x
        if (pos == Position::TOP_CENTER || pos == Position::MID_LEFT ||
            pos == Position::MID_RIGHT || pos == Position::BOTTOM_CENTER) {
            return 2.0f;
        }
        // Center: 1x
        return 1.0f;
    }

    static std::string getPositionName(Position pos) {
        static const char* names[] = {
            "Top Left", "Top Center", "Top Right",
            "Mid Left", "Center", "Mid Right",
            "Bottom Left", "Bottom Center", "Bottom Right"
        };
        return names[static_cast<int>(pos)];
    }
};

// Accuracy Challenge configuration
struct AccuracyChallengeConfig {
    float timeLimitSeconds = 60.0f;
    int32_t maxAttempts = 15;  // Optional attempt limit

    // 3x3 grid setup
    std::vector<TargetZone> targetZones;

    // Bonus for hitting all zones
    int32_t completionBonus = 1000;

    // Required accuracy (0-1)
    float minimumAccuracyForPass = 0.5f;

    ScoringConfig scoring;

    AccuracyChallengeConfig() {
        // Initialize 3x3 grid
        for (int i = 0; i < 9; i++) {
            TargetZone zone;
            zone.position = static_cast<TargetZone::Position>(i);
            zone.scoreMultiplier = TargetZone::getMultiplierForPosition(zone.position);
            zone.isHit = false;
            targetZones.push_back(zone);
        }

        scoring.accuracyMultiplier = 2.0f;
    }
};

// Power Challenge configuration
struct PowerChallengeConfig {
    int32_t maxAttempts = 3;

    // Velocity thresholds (km/h)
    float minimumVelocity = 40.0f;
    float goodVelocity = 70.0f;
    float excellentVelocity = 100.0f;
    float worldClassVelocity = 120.0f;

    // Scoring
    int32_t pointsPerKmh = 10;
    int32_t bonusExcellent = 500;
    int32_t bonusWorldClass = 1500;

    ScoringConfig scoring;

    PowerChallengeConfig() {
        scoring.powerMultiplier = 2.0f;
    }
};

// Penalty Shootout configuration
struct PenaltyShootoutConfig {
    int32_t kicksPerPlayer = 5;
    bool enableSuddenDeath = true;

    // Goalkeeper AI
    float goalkeeperReactionTime = 0.3f;  // seconds
    float goalkeeperCoverage = 0.7f;      // 0-1, ability to block
    float goalkeeperRandomness = 0.2f;    // 0-1, unpredictability

    // Scoring
    int32_t pointsPerGoal = 200;
    int32_t bonusCleanSheet = 1000;  // All 5 scored

    ScoringConfig scoring;
};

// Achievement thresholds
struct AchievementConfig {
    std::string id;
    std::string name;
    std::string description;
    std::string iconPath;

    // Unlock conditions
    ChallengeType challengeType;
    int32_t requiredScore = 0;
    int32_t requiredAttempts = 0;
    float requiredAccuracy = 0.0f;
    float requiredVelocity = 0.0f;

    bool isUnlocked = false;
};

// Predefined achievements
class AchievementRegistry {
public:
    static std::vector<AchievementConfig> getDefaultAchievements() {
        return {
            // Accuracy achievements
            {
                "bullseye",
                "Bullseye",
                "Hit all 9 target zones in one session",
                "assets/achievements/bullseye.png",
                ChallengeType::ACCURACY,
                0, 0, 1.0f, 0.0f
            },
            {
                "corner_specialist",
                "Corner Specialist",
                "Hit all 4 corners in accuracy challenge",
                "assets/achievements/corner_specialist.png",
                ChallengeType::ACCURACY,
                0, 0, 0.0f, 0.0f
            },
            {
                "sharpshooter",
                "Sharpshooter",
                "Achieve 80% accuracy with 10+ kicks",
                "assets/achievements/sharpshooter.png",
                ChallengeType::ACCURACY,
                0, 10, 0.8f, 0.0f
            },

            // Power achievements
            {
                "thunderstrike",
                "Thunderstrike",
                "Kick at 100+ km/h",
                "assets/achievements/thunderstrike.png",
                ChallengeType::POWER,
                0, 0, 0.0f, 100.0f
            },
            {
                "rocket_shot",
                "Rocket Shot",
                "Kick at 120+ km/h (world class)",
                "assets/achievements/rocket_shot.png",
                ChallengeType::POWER,
                0, 0, 0.0f, 120.0f
            },
            {
                "consistent_power",
                "Consistent Power",
                "Three consecutive 80+ km/h kicks",
                "assets/achievements/consistent_power.png",
                ChallengeType::POWER,
                0, 3, 0.0f, 80.0f
            },

            // Penalty achievements
            {
                "perfect_five",
                "Perfect Five",
                "Score all 5 penalties in a shootout",
                "assets/achievements/perfect_five.png",
                ChallengeType::PENALTY_SHOOTOUT,
                0, 5, 1.0f, 0.0f
            },
            {
                "ice_cold",
                "Ice Cold",
                "Win penalty shootout in sudden death",
                "assets/achievements/ice_cold.png",
                ChallengeType::PENALTY_SHOOTOUT,
                0, 0, 0.0f, 0.0f
            },
            {
                "penalty_master",
                "Penalty Master",
                "Score 20+ penalties total",
                "assets/achievements/penalty_master.png",
                ChallengeType::PENALTY_SHOOTOUT,
                0, 20, 0.0f, 0.0f
            }
        };
    }
};

// Global game configuration
struct GameConfig {
    // Display settings
    int32_t screenWidth = 1920;
    int32_t screenHeight = 1080;
    bool fullscreen = true;

    // Timing
    float countdownDuration = 3.0f;  // seconds
    float instructionsDuration = 5.0f;
    float resultsDuration = 10.0f;

    // Challenge configs
    AccuracyChallengeConfig accuracyConfig;
    PowerChallengeConfig powerConfig;
    PenaltyShootoutConfig penaltyConfig;

    // Achievements
    std::vector<AchievementConfig> achievements;

    // Session limits
    int32_t maxChallengesPerSession = 10;
    float sessionTimeoutMinutes = 5.0f;

    GameConfig() {
        achievements = AchievementRegistry::getDefaultAchievements();
    }
};

} // namespace game
} // namespace kinect

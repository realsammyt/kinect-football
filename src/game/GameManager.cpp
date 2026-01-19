#include "GameManager.h"
#include "AccuracyChallenge.h"
#include "PowerChallenge.h"
#include "PenaltyShootout.h"
#include "ScoringEngine.h"

namespace kinect {
namespace game {

GameManager::GameManager(const GameConfig& config)
    : config_(config)
    , sessionActive_(false)
{
}

GameManager::~GameManager() {
    shutdown();
}

void GameManager::initialize() {
    sessionActive_ = false;
    currentChallenge_.reset();
}

void GameManager::shutdown() {
    if (currentChallenge_) {
        currentChallenge_->finish();
        currentChallenge_.reset();
    }
    sessionActive_ = false;
}

bool GameManager::startChallenge(ChallengeType type) {
    // End current challenge if any
    if (currentChallenge_) {
        stopCurrentChallenge();
    }

    // Create new challenge
    currentChallenge_ = createChallenge(type);
    if (!currentChallenge_) {
        return false;
    }

    // Start challenge
    currentChallenge_->start();

    // Callback
    if (onChallengeStart_) {
        onChallengeStart_(type);
    }

    return true;
}

void GameManager::stopCurrentChallenge() {
    if (!currentChallenge_) {
        return;
    }

    // Finish if not already complete
    if (!currentChallenge_->isComplete()) {
        currentChallenge_->finish();
    }

    // Get result
    auto result = currentChallenge_->getResult();

    // Update session stats
    updateSessionStats(result);

    // Check achievements
    checkAchievements(result);

    // Callback
    if (onChallengeComplete_) {
        onChallengeComplete_(result);
    }

    // Clear challenge
    currentChallenge_.reset();
}

void GameManager::pauseCurrentChallenge() {
    if (currentChallenge_ && currentChallenge_->isActive()) {
        currentChallenge_->setState(ChallengeState::PAUSED);
    }
}

void GameManager::resumeCurrentChallenge() {
    if (currentChallenge_ && currentChallenge_->getState() == ChallengeState::PAUSED) {
        currentChallenge_->setState(ChallengeState::ACTIVE);
    }
}

void GameManager::processFrame(const k4abt_skeleton_t& skeleton,
                               const k4a_image_t& depthImage,
                               float deltaTime)
{
    if (!currentChallenge_) {
        return;
    }

    // Process frame
    currentChallenge_->processFrame(skeleton, depthImage, deltaTime);

    // Check if challenge completed
    if (currentChallenge_->isComplete()) {
        stopCurrentChallenge();
    }
}

void GameManager::render(cv::Mat& frame) {
    if (currentChallenge_) {
        currentChallenge_->render(frame);
    }
}

bool GameManager::hasActiveChallenge() const {
    return currentChallenge_ != nullptr;
}

ChallengeType GameManager::getCurrentChallengeType() const {
    return currentChallenge_ ? currentChallenge_->getType() : ChallengeType::ACCURACY;
}

ChallengeState GameManager::getCurrentChallengeState() const {
    return currentChallenge_ ? currentChallenge_->getState() : ChallengeState::IDLE;
}

void GameManager::startSession() {
    sessionActive_ = true;
    sessionStats_ = SessionStats();
    sessionStartTime_ = std::chrono::steady_clock::now();
}

void GameManager::endSession() {
    if (!sessionActive_) {
        return;
    }

    // Calculate session duration
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - sessionStartTime_
    );
    sessionStats_.sessionDuration = static_cast<float>(duration.count());

    sessionActive_ = false;
}

std::unique_ptr<ChallengeBase> GameManager::createChallenge(ChallengeType type) {
    switch (type) {
        case ChallengeType::ACCURACY:
            return std::make_unique<AccuracyChallenge>(config_.accuracyConfig);

        case ChallengeType::POWER:
            return std::make_unique<PowerChallenge>(config_.powerConfig);

        case ChallengeType::PENALTY_SHOOTOUT:
            return std::make_unique<PenaltyShootout>(config_.penaltyConfig);

        default:
            return nullptr;
    }
}

void GameManager::updateSessionStats(const ChallengeResult& result) {
    sessionStats_.totalScore += result.finalScore;
    sessionStats_.challengesCompleted++;
    sessionStats_.totalKicks += result.attempts;

    // Update average accuracy
    float totalAccuracy = sessionStats_.avgAccuracy *
                         (sessionStats_.challengesCompleted - 1);
    sessionStats_.avgAccuracy = (totalAccuracy + result.accuracy) /
                                sessionStats_.challengesCompleted;

    // Update max velocity
    if (result.maxVelocity > sessionStats_.maxVelocity) {
        sessionStats_.maxVelocity = result.maxVelocity;
    }

    // Update best scores
    auto& bestScore = sessionStats_.bestScores[result.type];
    if (result.finalScore > bestScore) {
        bestScore = result.finalScore;
    }
}

void GameManager::checkAchievements(const ChallengeResult& result) {
    switch (result.type) {
        case ChallengeType::ACCURACY:
            checkAccuracyAchievements(result);
            break;

        case ChallengeType::POWER:
            checkPowerAchievements(result);
            break;

        case ChallengeType::PENALTY_SHOOTOUT:
            checkPenaltyAchievements(result);
            break;

        default:
            break;
    }
}

void GameManager::checkAccuracyAchievements(const ChallengeResult& result) {
    // Get accuracy challenge for zone data
    auto* accuracyChallenge = dynamic_cast<AccuracyChallenge*>(currentChallenge_.get());
    if (!accuracyChallenge) {
        return;
    }

    const auto& zones = accuracyChallenge->getTargetZones();

    // Bullseye - all zones hit
    if (AchievementChecker::checkBullseye(result, zones)) {
        unlockAchievement("bullseye");
    }

    // Corner Specialist
    if (AchievementChecker::checkCornerSpecialist(zones)) {
        unlockAchievement("corner_specialist");
    }

    // Sharpshooter
    if (AchievementChecker::checkSharpshooter(result)) {
        unlockAchievement("sharpshooter");
    }
}

void GameManager::checkPowerAchievements(const ChallengeResult& result) {
    // Thunderstrike - 100+ km/h
    if (AchievementChecker::checkThunderstrike(result.maxVelocity)) {
        unlockAchievement("thunderstrike");
    }

    // Rocket Shot - 120+ km/h
    if (AchievementChecker::checkRocketShot(result.maxVelocity)) {
        unlockAchievement("rocket_shot");
    }

    // Consistent Power - track recent kicks
    lifetimeStats_.recentPowerKicks.push_back(result.maxVelocity);
    if (lifetimeStats_.recentPowerKicks.size() > 3) {
        lifetimeStats_.recentPowerKicks.erase(lifetimeStats_.recentPowerKicks.begin());
    }

    if (AchievementChecker::checkConsistentPower(lifetimeStats_.recentPowerKicks)) {
        unlockAchievement("consistent_power");
    }
}

void GameManager::checkPenaltyAchievements(const ChallengeResult& result) {
    // Track lifetime penalty goals
    lifetimeStats_.totalPenaltiesScored += result.successes;

    // Perfect Five
    if (AchievementChecker::checkPerfectFive(result.successes, result.attempts)) {
        unlockAchievement("perfect_five");
    }

    // Ice Cold - sudden death win
    auto* penaltyChallenge = dynamic_cast<PenaltyShootout*>(currentChallenge_.get());
    if (penaltyChallenge && penaltyChallenge->isSuddenDeath() && result.passed) {
        lifetimeStats_.suddenDeathWon = true;
        unlockAchievement("ice_cold");
    }

    // Penalty Master
    if (AchievementChecker::checkPenaltyMaster(lifetimeStats_.totalPenaltiesScored)) {
        unlockAchievement("penalty_master");
    }
}

void GameManager::unlockAchievement(const std::string& achievementId) {
    // Find achievement
    for (auto& achievement : config_.achievements) {
        if (achievement.id == achievementId && !achievement.isUnlocked) {
            achievement.isUnlocked = true;
            sessionStats_.achievementsUnlocked.push_back(achievementId);

            // Callback
            if (onAchievementUnlocked_) {
                onAchievementUnlocked_(achievement);
            }

            break;
        }
    }
}

bool GameManager::isAchievementUnlocked(const std::string& achievementId) const {
    for (const auto& achievement : config_.achievements) {
        if (achievement.id == achievementId) {
            return achievement.isUnlocked;
        }
    }
    return false;
}

std::vector<AchievementConfig> GameManager::getUnlockedAchievements() const {
    std::vector<AchievementConfig> unlocked;

    for (const auto& achievement : config_.achievements) {
        if (achievement.isUnlocked) {
            unlocked.push_back(achievement);
        }
    }

    return unlocked;
}

std::vector<AchievementConfig> GameManager::getAllAchievements() const {
    return config_.achievements;
}

} // namespace game
} // namespace kinect

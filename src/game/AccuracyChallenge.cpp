#include "AccuracyChallenge.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace kinect {
namespace game {

AccuracyChallenge::AccuracyChallenge(const AccuracyChallengeConfig& config)
    : ChallengeBase(ChallengeType::ACCURACY)
    , config_(config)
    , activeTarget_(TargetZone::Position::TOP_LEFT)
    , consecutiveHits_(0)
    , lastKickTime_(0.0f)
    , kickState_(KickState::IDLE)
    , kickPhaseTimer_(0.0f)
{
    targetZones_ = config_.targetZones;
}

void AccuracyChallenge::start() {
    ChallengeBase::start();

    // Randomize first target
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 8);
    activeTarget_ = static_cast<TargetZone::Position>(dis(gen));
}

void AccuracyChallenge::processFrame(const k4abt_skeleton_t& skeleton,
                                     const k4a_image_t& depthImage,
                                     float deltaTime)
{
    ChallengeBase::processFrame(skeleton, depthImage, deltaTime);

    if (state_ != ChallengeState::ACTIVE) {
        return;
    }

    // Check time limit
    float remaining = getRemainingTime(config_.timeLimitSeconds);
    if (remaining <= 0.0f) {
        finish();
        return;
    }

    // Check attempt limit
    if (config_.maxAttempts > 0 && totalAttempts_ >= config_.maxAttempts) {
        finish();
        return;
    }

    // Detect kicks
    detectKick(skeleton, deltaTime);
}

void AccuracyChallenge::finish() {
    // Calculate final score with completion bonus
    int zonesHit = 0;
    for (const auto& zone : targetZones_) {
        if (zone.isHit) zonesHit++;
    }

    if (zonesHit == 9) {
        addScore(config_.completionBonus);
    }

    // Update result
    result_.passed = result_.accuracy >= config_.minimumAccuracyForPass;

    // Calculate max score for grading
    int32_t maxPossibleScore = config_.scoring.basePoints * config_.maxAttempts *
                               config_.scoring.accuracyMultiplier * 3.0f +
                               config_.completionBonus;
    result_.grade = calculateGrade(currentScore_, maxPossibleScore);

    ChallengeBase::finish();
}

void AccuracyChallenge::reset() {
    ChallengeBase::reset();

    kickHistory_.clear();
    consecutiveHits_ = 0;
    lastKickTime_ = 0.0f;
    kickState_ = KickState::IDLE;

    // Reset all target zones
    for (auto& zone : targetZones_) {
        zone.isHit = false;
    }
}

void AccuracyChallenge::detectKick(const k4abt_skeleton_t& skeleton, float deltaTime) {
    // Get foot position (prefer right foot)
    k4a_float3_t footPos = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;

    // Simple kick detection based on foot velocity
    if (kickState_ == KickState::IDLE) {
        // Check for wind-up (foot moving back)
        float deltaZ = footPos.v[2] - lastFootPosition_.v[2];
        if (deltaZ > 0.1f) {  // Moving away from camera
            kickState_ = KickState::WINDING_UP;
            kickPhaseTimer_ = 0.0f;
        }
    }
    else if (kickState_ == KickState::WINDING_UP) {
        kickPhaseTimer_ += deltaTime;

        // Check for forward kick motion
        float deltaZ = footPos.v[2] - lastFootPosition_.v[2];
        if (deltaZ < -0.15f) {  // Moving toward camera fast
            kickState_ = KickState::KICKING;

            // Estimate trajectory and record kick
            k4a_float3_t trajectory = estimateBallTrajectory(skeleton);
            TargetZone::Position hitZone = determineHitZone(trajectory);

            KickData kick;
            kick.impactPoint = trajectory;
            kick.targetZone = hitZone;
            kick.velocity = std::sqrt(
                deltaZ * deltaZ +
                std::pow(footPos.v[0] - lastFootPosition_.v[0], 2) +
                std::pow(footPos.v[1] - lastFootPosition_.v[1], 2)
            ) / deltaTime;
            kick.onTarget = (hitZone == activeTarget_);
            kick.accuracy = kick.onTarget ? 1.0f : 0.0f;
            kick.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

            recordKick(kick);
        }
        else if (kickPhaseTimer_ > 1.0f) {
            // Reset if wind-up took too long
            kickState_ = KickState::IDLE;
        }
    }
    else if (kickState_ == KickState::KICKING) {
        kickPhaseTimer_ += deltaTime;
        if (kickPhaseTimer_ > 0.3f) {
            kickState_ = KickState::FOLLOW_THROUGH;
        }
    }
    else if (kickState_ == KickState::FOLLOW_THROUGH) {
        kickPhaseTimer_ += deltaTime;
        if (kickPhaseTimer_ > 0.5f) {
            kickState_ = KickState::IDLE;
        }
    }

    lastFootPosition_ = footPos;
}

bool AccuracyChallenge::isKickingPose(const k4abt_skeleton_t& skeleton) {
    // Check if foot is behind and elevated (wind-up)
    auto foot = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;
    auto knee = skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].position;

    return foot.v[2] > knee.v[2] && foot.v[1] > knee.v[1] - 0.1f;
}

k4a_float3_t AccuracyChallenge::estimateBallTrajectory(const k4abt_skeleton_t& skeleton) {
    // Simple trajectory estimation based on foot direction
    auto foot = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;
    auto knee = skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].position;

    k4a_float3_t direction;
    direction.v[0] = foot.v[0] - knee.v[0];
    direction.v[1] = foot.v[1] - knee.v[1];
    direction.v[2] = foot.v[2] - knee.v[2];

    // Normalize
    float length = std::sqrt(
        direction.v[0] * direction.v[0] +
        direction.v[1] * direction.v[1] +
        direction.v[2] * direction.v[2]
    );

    if (length > 0.001f) {
        direction.v[0] /= length;
        direction.v[1] /= length;
        direction.v[2] /= length;
    }

    // Project to goal plane (assume 5m away)
    float distanceToGoal = 5.0f;
    k4a_float3_t impact;
    impact.v[0] = foot.v[0] + direction.v[0] * distanceToGoal;
    impact.v[1] = foot.v[1] + direction.v[1] * distanceToGoal;
    impact.v[2] = foot.v[2] + direction.v[2] * distanceToGoal;

    return impact;
}

TargetZone::Position AccuracyChallenge::determineHitZone(const k4a_float3_t& impactPoint) {
    // Map impact point to 3x3 grid
    // Assume goal is 2.44m high, 7.32m wide (standard soccer goal)
    // Center of goal is at x=0, y=1.22m

    float goalWidth = 7.32f;
    float goalHeight = 2.44f;
    float goalCenterY = 1.22f;

    // Relative position in goal (-1 to 1)
    float relX = (impactPoint.v[0]) / (goalWidth / 2.0f);
    float relY = (impactPoint.v[1] - goalCenterY) / (goalHeight / 2.0f);

    // Clamp to goal bounds
    relX = std::max(-1.0f, std::min(1.0f, relX));
    relY = std::max(-1.0f, std::min(1.0f, relY));

    // Map to grid position
    int gridX = relX < -0.33f ? 0 : (relX > 0.33f ? 2 : 1);
    int gridY = relY > 0.33f ? 0 : (relY < -0.33f ? 2 : 1);

    return static_cast<TargetZone::Position>(gridY * 3 + gridX);
}

void AccuracyChallenge::recordKick(const KickData& kick) {
    kickHistory_.push_back(kick);
    recordAttempt(kick.onTarget);

    // Update target zone
    if (kick.onTarget) {
        targetZones_[static_cast<int>(kick.targetZone)].isHit = true;
    }

    // Calculate score
    int32_t kickScore = calculateKickScore(kick);
    addScore(kickScore);

    // Check combo bonus
    checkComboBonus();

    // Select next target (prioritize unhit zones)
    std::vector<int> unhitZones;
    for (int i = 0; i < 9; i++) {
        if (!targetZones_[i].isHit) {
            unhitZones.push_back(i);
        }
    }

    if (!unhitZones.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, unhitZones.size() - 1);
        activeTarget_ = static_cast<TargetZone::Position>(unhitZones[dis(gen)]);
    }
}

int32_t AccuracyChallenge::calculateKickScore(const KickData& kick) {
    if (!kick.onTarget) {
        return 0;
    }

    float zoneMultiplier = targetZones_[static_cast<int>(kick.targetZone)].scoreMultiplier;
    int32_t baseScore = config_.scoring.basePoints;

    return static_cast<int32_t>(
        baseScore * zoneMultiplier * config_.scoring.accuracyMultiplier
    );
}

void AccuracyChallenge::checkComboBonus() {
    // Check recent kicks for streak
    int recentHits = 0;
    size_t checkCount = std::min(kickHistory_.size(), size_t(5));

    for (size_t i = kickHistory_.size() - checkCount; i < kickHistory_.size(); i++) {
        if (kickHistory_[i].onTarget) {
            recentHits++;
        }
    }

    if (recentHits >= config_.scoring.streakThreshold) {
        int32_t streakBonus = static_cast<int32_t>(
            config_.scoring.basePoints *
            config_.scoring.comboMultiplier *
            (recentHits - config_.scoring.streakThreshold + 1)
        );
        addScore(streakBonus);
    }
}

void AccuracyChallenge::setActiveTarget(TargetZone::Position target) {
    activeTarget_ = target;
}

void AccuracyChallenge::render(cv::Mat& frame) {
    switch (state_) {
        case ChallengeState::INSTRUCTIONS:
            renderInstructions(frame);
            break;
        case ChallengeState::COUNTDOWN:
            renderCountdown(frame);
            break;
        case ChallengeState::ACTIVE:
            renderTargetGrid(frame);
            renderActiveTarget(frame);
            renderStats(frame);
            renderKickTrajectory(frame);
            break;
        case ChallengeState::COMPLETE:
            renderResults(frame);
            break;
        default:
            break;
    }
}

void AccuracyChallenge::renderTargetGrid(cv::Mat& frame) {
    // Draw 3x3 grid overlay on goal
    int gridWidth = 300;
    int gridHeight = 200;
    int startX = frame.cols - gridWidth - 50;
    int startY = 50;

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int x = startX + col * (gridWidth / 3);
            int y = startY + row * (gridHeight / 3);
            int w = gridWidth / 3;
            int h = gridHeight / 3;

            auto position = static_cast<TargetZone::Position>(row * 3 + col);
            const auto& zone = targetZones_[row * 3 + col];

            cv::Scalar color = zone.isHit
                ? cv::Scalar(0, 255, 0)  // Green if hit
                : cv::Scalar(100, 100, 100);  // Gray if not hit

            cv::rectangle(frame, cv::Point(x, y), cv::Point(x + w, y + h),
                         color, zone.isHit ? -1 : 2);

            // Show multiplier
            std::string mult = std::to_string(static_cast<int>(zone.scoreMultiplier)) + "x";
            cv::putText(frame, mult, cv::Point(x + 10, y + h - 10),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        }
    }
}

void AccuracyChallenge::renderActiveTarget(cv::Mat& frame) {
    // Highlight active target
    int gridWidth = 300;
    int gridHeight = 200;
    int startX = frame.cols - gridWidth - 50;
    int startY = 50;

    int row = static_cast<int>(activeTarget_) / 3;
    int col = static_cast<int>(activeTarget_) % 3;

    int x = startX + col * (gridWidth / 3);
    int y = startY + row * (gridHeight / 3);
    int w = gridWidth / 3;
    int h = gridHeight / 3;

    // Pulsing highlight
    float pulse = 0.5f + 0.5f * std::sin(getElapsedTime() * 4.0f);
    cv::Scalar highlightColor(0, 255 * pulse, 255);

    cv::rectangle(frame, cv::Point(x - 5, y - 5),
                 cv::Point(x + w + 5, y + h + 5),
                 highlightColor, 4);
}

void AccuracyChallenge::renderStats(cv::Mat& frame) {
    int yPos = 50;

    // Time remaining
    float remaining = getRemainingTime(config_.timeLimitSeconds);
    std::string timeText = "Time: " + std::to_string(static_cast<int>(remaining)) + "s";
    cv::putText(frame, timeText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 255), 2);
    yPos += 50;

    // Score
    std::string scoreText = "Score: " + std::to_string(currentScore_);
    cv::putText(frame, scoreText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 255), 2);
    yPos += 50;

    // Accuracy
    float accuracy = totalAttempts_ > 0
        ? static_cast<float>(successfulAttempts_) / totalAttempts_
        : 0.0f;
    std::string accText = "Accuracy: " + std::to_string(static_cast<int>(accuracy * 100)) + "%";
    cv::putText(frame, accText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(0, 255, 0), 2);
    yPos += 50;

    // Zones hit
    int zonesHit = 0;
    for (const auto& zone : targetZones_) {
        if (zone.isHit) zonesHit++;
    }
    std::string zonesText = "Zones: " + std::to_string(zonesHit) + "/9";
    cv::putText(frame, zonesText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 0), 2);
}

void AccuracyChallenge::renderKickTrajectory(cv::Mat& frame) {
    // Draw recent kick trajectory if available
    if (!kickHistory_.empty() && kickHistory_.back().timestamp > 0) {
        auto lastKick = kickHistory_.back();
        uint64_t now = std::chrono::steady_clock::now().time_since_epoch().count();
        uint64_t age = now - lastKick.timestamp;

        // Only show for 1 second after kick
        if (age < 1000000000) {  // 1 billion nanoseconds = 1 second
            cv::Scalar color = lastKick.onTarget
                ? cv::Scalar(0, 255, 0)
                : cv::Scalar(0, 0, 255);

            // Show trajectory line (simplified)
            cv::line(frame,
                    cv::Point(frame.cols / 2, frame.rows - 100),
                    cv::Point(frame.cols - 200, 300),
                    color, 3);

            std::string result = lastKick.onTarget ? "HIT!" : "MISS";
            cv::putText(frame, result, cv::Point(frame.cols / 2 - 50, frame.rows - 150),
                       cv::FONT_HERSHEY_BOLD, 2.0, color, 3);
        }
    }
}

} // namespace game
} // namespace kinect

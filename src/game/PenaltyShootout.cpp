#include "PenaltyShootout.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace game {

// GoalkeeperAI implementation
GoalkeeperAI::GoalkeeperAI(float reactionTime, float coverage, float randomness)
    : reactionTime_(reactionTime)
    , coverage_(coverage)
    , randomness_(randomness)
    , lastDive_(TargetZone::Position::MID_CENTER)
{
    std::random_device rd;
    rng_.seed(rd());
}

TargetZone::Position GoalkeeperAI::predictDive(const k4abt_skeleton_t& skeleton,
                                                const k4a_float3_t& kickDirection)
{
    // Add randomness to prediction
    std::uniform_real_distribution<float> randomDist(0.0f, 1.0f);

    if (randomDist(rng_) < randomness_) {
        // Random dive
        std::uniform_int_distribution<int> zoneDist(0, 8);
        return static_cast<TargetZone::Position>(zoneDist(rng_));
    }

    // Predict based on kick direction
    // Simplified: map kick direction to zone
    float angleX = std::atan2(kickDirection.v[0], -kickDirection.v[2]);
    float angleY = std::atan2(kickDirection.v[1], -kickDirection.v[2]);

    int gridX = angleX < -0.3f ? 0 : (angleX > 0.3f ? 2 : 1);
    int gridY = angleY > 0.3f ? 0 : (angleY < -0.3f ? 2 : 1);

    lastDive_ = static_cast<TargetZone::Position>(gridY * 3 + gridX);
    return lastDive_;
}

bool GoalkeeperAI::willSave(const TargetZone::Position& kickZone,
                            const TargetZone::Position& diveZone,
                            float velocity)
{
    // If goalkeeper dives to correct zone
    if (kickZone == diveZone) {
        // Higher velocity reduces save chance
        float velocityFactor = std::max(0.0f, 1.0f - (velocity - 10.0f) / 20.0f);

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < (coverage_ * velocityFactor);
    }

    // Adjacent zones have lower save chance
    int kickIdx = static_cast<int>(kickZone);
    int diveIdx = static_cast<int>(diveZone);

    int kickRow = kickIdx / 3;
    int kickCol = kickIdx % 3;
    int diveRow = diveIdx / 3;
    int diveCol = diveIdx % 3;

    int distance = std::abs(kickRow - diveRow) + std::abs(kickCol - diveCol);

    if (distance == 1) {
        // Adjacent zone
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < (coverage_ * 0.3f);
    }

    return false;  // Too far to save
}

void GoalkeeperAI::reset() {
    lastDive_ = TargetZone::Position::MID_CENTER;
}

// PenaltyShootout implementation
PenaltyShootout::PenaltyShootout(const PenaltyShootoutConfig& config)
    : ChallengeBase(ChallengeType::PENALTY_SHOOTOUT)
    , config_(config)
    , currentRound_(0)
    , goalsScored_(0)
    , goalsMissed_(0)
    , suddenDeath_(false)
    , goalkeeperDive_(TargetZone::Position::MID_CENTER)
    , goalkeeperAnimationTime_(0.0f)
    , penaltyState_(PenaltyState::POSITIONING)
    , stateTimer_(0.0f)
    , lastResult_(PenaltyKick::Result::PENDING)
    , resultAnimationTime_(0.0f)
{
    goalkeeper_ = std::make_unique<GoalkeeperAI>(
        config_.goalkeeperReactionTime,
        config_.goalkeeperCoverage,
        config_.goalkeeperRandomness
    );
}

void PenaltyShootout::start() {
    ChallengeBase::start();
    goalkeeper_->reset();
    penaltyState_ = PenaltyState::POSITIONING;
}

void PenaltyShootout::processFrame(const k4abt_skeleton_t& skeleton,
                                   const k4a_image_t& depthImage,
                                   float deltaTime)
{
    ChallengeBase::processFrame(skeleton, depthImage, deltaTime);

    if (state_ != ChallengeState::ACTIVE) {
        return;
    }

    stateTimer_ += deltaTime;

    switch (penaltyState_) {
        case PenaltyState::POSITIONING:
            // Wait for player to be ready
            if (stateTimer_ > 2.0f) {
                penaltyState_ = PenaltyState::AIMING;
                stateTimer_ = 0.0f;
            }
            break;

        case PenaltyState::AIMING:
            // Show aiming guide, wait for kick
            detectPenaltyKick(skeleton, deltaTime);
            break;

        case PenaltyState::WINDUP:
            // Detecting kick motion
            detectPenaltyKick(skeleton, deltaTime);
            break;

        case PenaltyState::KICKED:
            updateGoalkeeper(deltaTime);

            if (stateTimer_ > 1.5f) {
                penaltyState_ = PenaltyState::RESULT_SHOW;
                stateTimer_ = 0.0f;
                resultAnimationTime_ = 0.0f;
            }
            break;

        case PenaltyState::RESULT_SHOW:
            resultAnimationTime_ += deltaTime;

            if (stateTimer_ > 3.0f) {
                advanceRound();
            }
            break;

        case PenaltyState::NEXT_ROUND:
            if (stateTimer_ > 1.0f) {
                if (currentRound_ >= config_.kicksPerPlayer) {
                    if (config_.enableSuddenDeath && goalsScored_ == goalsMissed_) {
                        // Sudden death
                        suddenDeath_ = true;
                        currentRound_ = 0;
                        penaltyState_ = PenaltyState::POSITIONING;
                    } else {
                        finish();
                    }
                } else {
                    penaltyState_ = PenaltyState::POSITIONING;
                    stateTimer_ = 0.0f;
                }
            }
            break;
    }
}

void PenaltyShootout::finish() {
    // Calculate final score
    int32_t baseScore = goalsScored_ * config_.pointsPerGoal;

    // Bonus for clean sheet
    if (goalsScored_ == config_.kicksPerPlayer) {
        baseScore += config_.bonusCleanSheet;
    }

    currentScore_ = baseScore;
    result_.passed = goalsScored_ > goalsMissed_;

    // Grading
    int32_t maxScore = config_.kicksPerPlayer * config_.pointsPerGoal +
                       config_.bonusCleanSheet;
    result_.grade = calculateGrade(currentScore_, maxScore);

    ChallengeBase::finish();
}

void PenaltyShootout::reset() {
    ChallengeBase::reset();

    kicks_.clear();
    currentRound_ = 0;
    goalsScored_ = 0;
    goalsMissed_ = 0;
    suddenDeath_ = false;
    penaltyState_ = PenaltyState::POSITIONING;
    stateTimer_ = 0.0f;
    goalkeeper_->reset();
    footTrajectory_.clear();
}

void PenaltyShootout::detectPenaltyKick(const k4abt_skeleton_t& skeleton, float deltaTime) {
    k4a_float3_t footPos = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;

    footTrajectory_.push_back(footPos);
    if (footTrajectory_.size() > 10) {
        footTrajectory_.erase(footTrajectory_.begin());
    }

    if (penaltyState_ == PenaltyState::AIMING) {
        // Look for wind-up
        if (footTrajectory_.size() >= 3) {
            auto current = footTrajectory_.back();
            auto prev = footTrajectory_[footTrajectory_.size() - 3];

            float deltaZ = current.v[2] - prev.v[2];
            if (deltaZ > 0.15f) {
                penaltyState_ = PenaltyState::WINDUP;
                stateTimer_ = 0.0f;
            }
        }
    }
    else if (penaltyState_ == PenaltyState::WINDUP) {
        // Look for forward kick
        if (footTrajectory_.size() >= 5) {
            auto current = footTrajectory_.back();
            auto prev = footTrajectory_[footTrajectory_.size() - 5];

            float deltaZ = current.v[2] - prev.v[2];

            if (deltaZ < -0.2f) {  // Fast forward motion
                executePenalty(skeleton);
                penaltyState_ = PenaltyState::KICKED;
                stateTimer_ = 0.0f;
            }
        }
    }

    lastFootPosition_ = footPos;
}

k4a_float3_t PenaltyShootout::estimateKickDirection(const k4abt_skeleton_t& skeleton) {
    auto foot = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;
    auto knee = skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].position;

    k4a_float3_t direction;
    direction.v[0] = foot.v[0] - knee.v[0];
    direction.v[1] = foot.v[1] - knee.v[1];
    direction.v[2] = -(foot.v[2] - knee.v[2]);  // Toward goal

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

    return direction;
}

TargetZone::Position PenaltyShootout::determineTargetZone(const k4a_float3_t& direction) {
    float angleX = std::atan2(direction.v[0], direction.v[2]);
    float angleY = std::atan2(direction.v[1], direction.v[2]);

    int gridX = angleX < -0.3f ? 0 : (angleX > 0.3f ? 2 : 1);
    int gridY = angleY > 0.3f ? 0 : (angleY < -0.3f ? 2 : 1);

    return static_cast<TargetZone::Position>(gridY * 3 + gridX);
}

void PenaltyShootout::executePenalty(const k4abt_skeleton_t& skeleton) {
    PenaltyKick kick;
    kick.kickDirection = estimateKickDirection(skeleton);
    kick.targetZone = determineTargetZone(kick.kickDirection);

    // Calculate velocity from foot trajectory
    if (footTrajectory_.size() >= 5) {
        auto current = footTrajectory_.back();
        auto prev = footTrajectory_[footTrajectory_.size() - 5];

        float dx = current.v[0] - prev.v[0];
        float dy = current.v[1] - prev.v[1];
        float dz = current.v[2] - prev.v[2];

        kick.velocity = std::sqrt(dx * dx + dy * dy + dz * dz) / 0.16f;  // Approx 5 frames at 30fps
    } else {
        kick.velocity = 10.0f;  // Default
    }

    // Goalkeeper predicts and dives
    goalkeeperDive_ = goalkeeper_->predictDive(skeleton, kick.kickDirection);
    goalkeeperAnimationTime_ = 0.0f;

    // Determine if saved
    bool saved = checkSave(kick);

    if (saved) {
        kick.result = PenaltyKick::Result::SAVED;
        goalsMissed_++;
    } else {
        kick.result = PenaltyKick::Result::GOAL;
        goalsScored_++;
    }

    kick.score = kick.result == PenaltyKick::Result::GOAL
        ? config_.pointsPerGoal
        : 0;

    kick.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    recordPenalty(kick);
    lastResult_ = kick.result;
}

bool PenaltyShootout::checkSave(const PenaltyKick& kick) {
    return goalkeeper_->willSave(kick.targetZone, goalkeeperDive_, kick.velocity);
}

void PenaltyShootout::recordPenalty(PenaltyKick kick) {
    kicks_.push_back(kick);
    recordAttempt(kick.result == PenaltyKick::Result::GOAL);
    addScore(kick.score);
}

void PenaltyShootout::advanceRound() {
    currentRound_++;
    penaltyState_ = PenaltyState::NEXT_ROUND;
    stateTimer_ = 0.0f;
    footTrajectory_.clear();
}

void PenaltyShootout::updateGoalkeeper(float deltaTime) {
    goalkeeperAnimationTime_ += deltaTime;
}

void PenaltyShootout::render(cv::Mat& frame) {
    switch (state_) {
        case ChallengeState::INSTRUCTIONS:
            renderInstructions(frame);
            break;
        case ChallengeState::COUNTDOWN:
            renderCountdown(frame);
            break;
        case ChallengeState::ACTIVE:
            renderGoalkeeper(frame);
            renderScoreboard(frame);
            if (penaltyState_ == PenaltyState::AIMING) {
                renderAimingGuide(frame);
            }
            if (penaltyState_ == PenaltyState::RESULT_SHOW) {
                renderKickResult(frame);
            }
            break;
        case ChallengeState::COMPLETE:
            renderResults(frame);
            break;
        default:
            break;
    }
}

void PenaltyShootout::renderGoalkeeper(cv::Mat& frame) {
    // Draw goal
    int goalWidth = 600;
    int goalHeight = 400;
    int goalX = frame.cols - goalWidth - 100;
    int goalY = frame.rows / 2 - goalHeight / 2;

    cv::rectangle(frame, cv::Point(goalX, goalY),
                 cv::Point(goalX + goalWidth, goalY + goalHeight),
                 cv::Scalar(255, 255, 255), 3);

    // Draw goalkeeper
    int gkSize = 80;
    int gkX = goalX + goalWidth / 2;
    int gkY = goalY + goalHeight / 2;

    // Animate dive during kicked state
    if (penaltyState_ == PenaltyState::KICKED && goalkeeperAnimationTime_ < 1.0f) {
        int diveRow = static_cast<int>(goalkeeperDive_) / 3;
        int diveCol = static_cast<int>(goalkeeperDive_) % 3;

        int targetX = goalX + (goalWidth / 3) * diveCol + (goalWidth / 6);
        int targetY = goalY + (goalHeight / 3) * diveRow + (goalHeight / 6);

        float progress = goalkeeperAnimationTime_ / 0.8f;
        progress = std::min(1.0f, progress);

        gkX = static_cast<int>(gkX + (targetX - gkX) * progress);
        gkY = static_cast<int>(gkY + (targetY - gkY) * progress);
    }

    // Draw goalkeeper as circle
    cv::circle(frame, cv::Point(gkX, gkY), gkSize / 2,
              cv::Scalar(255, 200, 0), -1);
    cv::circle(frame, cv::Point(gkX, gkY), gkSize / 2,
              cv::Scalar(0, 0, 0), 2);
}

void PenaltyShootout::renderScoreboard(cv::Mat& frame) {
    int yPos = 50;

    // Round counter
    std::string roundText = suddenDeath_ ? "SUDDEN DEATH" :
                           "Round " + std::to_string(currentRound_ + 1) +
                           "/" + std::to_string(config_.kicksPerPlayer);
    cv::putText(frame, roundText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_BOLD, 1.5,
               suddenDeath_ ? cv::Scalar(255, 0, 0) : cv::Scalar(255, 255, 255), 3);
    yPos += 70;

    // Score
    std::string scoreText = "Goals: " + std::to_string(goalsScored_) +
                           " / " + std::to_string(currentRound_);
    cv::putText(frame, scoreText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.3, cv::Scalar(0, 255, 0), 2);
    yPos += 60;

    // Points
    std::string pointsText = "Points: " + std::to_string(currentScore_);
    cv::putText(frame, pointsText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.3, cv::Scalar(0, 255, 255), 2);
    yPos += 80;

    // Kick history
    int historyCount = std::min(5, static_cast<int>(kicks_.size()));
    for (int i = 0; i < historyCount; i++) {
        size_t idx = kicks_.size() - historyCount + i;
        const auto& kick = kicks_[idx];

        std::string symbol = kick.result == PenaltyKick::Result::GOAL ? "O" : "X";
        cv::Scalar color = kick.result == PenaltyKick::Result::GOAL
            ? cv::Scalar(0, 255, 0)
            : cv::Scalar(0, 0, 255);

        cv::circle(frame, cv::Point(70 + i * 50, yPos + 20), 15, color, -1);
        cv::putText(frame, symbol, cv::Point(63 + i * 50, yPos + 30),
                   cv::FONT_HERSHEY_BOLD, 0.8, cv::Scalar(255, 255, 255), 2);
    }
}

void PenaltyShootout::renderKickResult(cv::Mat& frame) {
    std::string resultText = lastResult_ == PenaltyKick::Result::GOAL
        ? "GOAL!"
        : "SAVED!";

    cv::Scalar color = lastResult_ == PenaltyKick::Result::GOAL
        ? cv::Scalar(0, 255, 0)
        : cv::Scalar(0, 0, 255);

    // Animate
    float scale = 1.0f + resultAnimationTime_ * 0.5f;
    float alpha = std::max(0.0f, 1.0f - resultAnimationTime_ * 0.5f);

    cv::Size textSize = cv::getTextSize(resultText, cv::FONT_HERSHEY_BOLD,
                                       5.0 * scale, static_cast<int>(8 * scale),
                                       nullptr);

    cv::Point pos(frame.cols / 2 - textSize.width / 2,
                 frame.rows / 2 + textSize.height / 2);

    cv::putText(frame, resultText, pos, cv::FONT_HERSHEY_BOLD,
               5.0 * scale, color, static_cast<int>(8 * scale * alpha));
}

void PenaltyShootout::renderAimingGuide(cv::Mat& frame) {
    // Draw aiming reticle
    std::string aimText = "Aim with your kick direction!";
    cv::putText(frame, aimText,
               cv::Point(frame.cols / 2 - 250, frame.rows - 100),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 0), 2);

    // Pulsing crosshair
    float pulse = 0.5f + 0.5f * std::sin(getElapsedTime() * 3.0f);
    cv::Scalar crosshairColor(0, 255 * pulse, 255);

    int centerX = frame.cols / 2;
    int centerY = frame.rows / 2;
    int size = 40;

    cv::line(frame, cv::Point(centerX - size, centerY),
            cv::Point(centerX + size, centerY), crosshairColor, 3);
    cv::line(frame, cv::Point(centerX, centerY - size),
            cv::Point(centerX, centerY + size), crosshairColor, 3);
}

} // namespace game
} // namespace kinect

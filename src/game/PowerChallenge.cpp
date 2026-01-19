#include "PowerChallenge.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace game {

PowerChallenge::PowerChallenge(const PowerChallengeConfig& config)
    : ChallengeBase(ChallengeType::POWER)
    , config_(config)
    , personalBest_(0.0f)
    , kickState_(PowerKickState::WAITING)
    , kickTimer_(0.0f)
    , kickAnimationProgress_(0.0f)
    , lastKickVelocity_(0.0f)
{
}

void PowerChallenge::start() {
    ChallengeBase::start();
    kickState_ = PowerKickState::WAITING;
}

void PowerChallenge::processFrame(const k4abt_skeleton_t& skeleton,
                                  const k4a_image_t& depthImage,
                                  float deltaTime)
{
    ChallengeBase::processFrame(skeleton, depthImage, deltaTime);

    if (state_ != ChallengeState::ACTIVE) {
        return;
    }

    // Check if all attempts used
    if (attempts_.size() >= static_cast<size_t>(config_.maxAttempts)) {
        finish();
        return;
    }

    // Track foot positions for velocity calculation
    k4a_float3_t footPos = skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position;
    footPositions_.push_back(footPos);
    footTimestamps_.push_back(getElapsedTime());

    if (footPositions_.size() > MAX_FOOT_HISTORY) {
        footPositions_.pop_front();
        footTimestamps_.pop_front();
    }

    // Detect power kicks
    detectPowerKick(skeleton, deltaTime);

    // Update animation
    if (kickAnimationProgress_ > 0.0f) {
        kickAnimationProgress_ -= deltaTime * 2.0f;
        if (kickAnimationProgress_ < 0.0f) {
            kickAnimationProgress_ = 0.0f;
        }
    }
}

void PowerChallenge::finish() {
    // Calculate final score
    updateResult();

    // Update personal best
    for (const auto& attempt : attempts_) {
        if (attempt.velocityKmh > personalBest_) {
            personalBest_ = attempt.velocityKmh;
        }
    }

    // Store max velocity in result
    result_.maxVelocity = personalBest_;

    // Calculate average
    float totalVelocity = 0.0f;
    for (const auto& attempt : attempts_) {
        totalVelocity += attempt.velocityKmh;
    }
    result_.avgVelocity = attempts_.empty() ? 0.0f : totalVelocity / attempts_.size();

    // Grading based on best kick
    int32_t maxScore = config_.pointsPerKmh * static_cast<int32_t>(config_.worldClassVelocity) +
                       config_.bonusWorldClass;
    result_.grade = calculateGrade(currentScore_, maxScore);

    result_.passed = personalBest_ >= config_.minimumVelocity;

    ChallengeBase::finish();
}

void PowerChallenge::reset() {
    ChallengeBase::reset();

    attempts_.clear();
    kickState_ = PowerKickState::WAITING;
    kickTimer_ = 0.0f;
    footPositions_.clear();
    footTimestamps_.clear();
    kickAnimationProgress_ = 0.0f;
    lastKickVelocity_ = 0.0f;
}

void PowerChallenge::detectPowerKick(const k4abt_skeleton_t& skeleton, float deltaTime) {
    kickTimer_ += deltaTime;

    switch (kickState_) {
        case PowerKickState::WAITING: {
            // Look for wind-up (foot moving back and up)
            if (footPositions_.size() >= 3) {
                auto current = footPositions_.back();
                auto prev = footPositions_[footPositions_.size() - 3];

                float deltaZ = current.v[2] - prev.v[2];
                float deltaY = current.v[1] - prev.v[1];

                if (deltaZ > 0.15f && deltaY > 0.05f) {  // Moving back and up
                    kickState_ = PowerKickState::WINDUP;
                    kickTimer_ = 0.0f;
                }
            }
            break;
        }

        case PowerKickState::WINDUP: {
            // Look for forward motion (impact)
            if (footPositions_.size() >= 5) {
                float velocity = calculateLegVelocity(skeleton);

                if (velocity > 3.0f) {  // Fast forward motion (m/s)
                    // Record the kick
                    PowerKickAttempt attempt;
                    attempt.legSpeed = velocity;
                    attempt.velocity = velocity;
                    attempt.velocityKmh = velocity * 3.6f;  // Convert m/s to km/h
                    attempt.technique = calculateTechnique(skeleton);
                    attempt.rating = getRating(attempt.velocityKmh);
                    attempt.timestamp = std::chrono::steady_clock::now()
                                       .time_since_epoch().count();

                    recordPowerKick(attempt);

                    kickState_ = PowerKickState::IMPACT;
                    kickTimer_ = 0.0f;
                    kickAnimationProgress_ = 1.0f;
                    lastKickVelocity_ = attempt.velocityKmh;
                }
            }

            // Timeout if wind-up takes too long
            if (kickTimer_ > 2.0f) {
                kickState_ = PowerKickState::WAITING;
            }
            break;
        }

        case PowerKickState::IMPACT: {
            if (kickTimer_ > 0.5f) {
                kickState_ = PowerKickState::COOLDOWN;
                kickTimer_ = 0.0f;
            }
            break;
        }

        case PowerKickState::COOLDOWN: {
            // Wait before allowing next attempt
            if (kickTimer_ > 2.0f) {
                kickState_ = PowerKickState::WAITING;
            }
            break;
        }
    }
}

float PowerChallenge::calculateLegVelocity(const k4abt_skeleton_t& skeleton) {
    if (footPositions_.size() < 5 || footTimestamps_.size() < 5) {
        return 0.0f;
    }

    // Calculate velocity over recent frames
    size_t frameCount = 5;
    auto endPos = footPositions_.back();
    auto startPos = footPositions_[footPositions_.size() - frameCount];

    float endTime = footTimestamps_.back();
    float startTime = footTimestamps_[footTimestamps_.size() - frameCount];

    float deltaTime = endTime - startTime;
    if (deltaTime < 0.001f) {
        return 0.0f;
    }

    float dx = endPos.v[0] - startPos.v[0];
    float dy = endPos.v[1] - startPos.v[1];
    float dz = endPos.v[2] - startPos.v[2];

    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return distance / deltaTime;
}

float PowerChallenge::calculateTechnique(const k4abt_skeleton_t& skeleton) {
    // Technique scoring based on form
    float score = 1.0f;

    // Check hip rotation
    auto hipLeft = skeleton.joints[K4ABT_JOINT_HIP_LEFT].position;
    auto hipRight = skeleton.joints[K4ABT_JOINT_HIP_RIGHT].position;
    float hipAngle = std::atan2(hipRight.v[0] - hipLeft.v[0],
                                hipRight.v[2] - hipLeft.v[2]);

    // Good hip rotation adds to technique
    if (std::abs(hipAngle) > 0.3f) {
        score *= 1.2f;
    }

    // Check knee bend
    auto knee = skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].position;
    auto ankle = skeleton.joints[K4ABT_JOINT_ANKLE_RIGHT].position;
    auto hip = skeleton.joints[K4ABT_JOINT_HIP_RIGHT].position;

    float thighLength = std::sqrt(
        std::pow(knee.v[0] - hip.v[0], 2) +
        std::pow(knee.v[1] - hip.v[1], 2) +
        std::pow(knee.v[2] - hip.v[2], 2)
    );

    float shinLength = std::sqrt(
        std::pow(ankle.v[0] - knee.v[0], 2) +
        std::pow(ankle.v[1] - knee.v[1], 2) +
        std::pow(ankle.v[2] - knee.v[2], 2)
    );

    // Good knee bend in wind-up
    if (thighLength > 0.001f && shinLength / thighLength < 0.8f) {
        score *= 1.15f;
    }

    return std::min(2.0f, score);  // Cap at 2x
}

std::string PowerChallenge::getRating(float velocityKmh) {
    if (velocityKmh >= config_.worldClassVelocity) {
        return "WORLD CLASS!";
    } else if (velocityKmh >= config_.excellentVelocity) {
        return "EXCELLENT!";
    } else if (velocityKmh >= config_.goodVelocity) {
        return "GOOD";
    } else if (velocityKmh >= config_.minimumVelocity) {
        return "WEAK";
    } else {
        return "TOO SLOW";
    }
}

void PowerChallenge::recordPowerKick(const PowerKickAttempt& attempt) {
    int32_t score = calculatePowerScore(attempt);

    PowerKickAttempt scored = attempt;
    scored.score = score;

    attempts_.push_back(scored);
    recordAttempt(attempt.velocityKmh >= config_.minimumVelocity);
    addScore(score);
}

int32_t PowerChallenge::calculatePowerScore(const PowerKickAttempt& attempt) {
    // Base score from velocity
    int32_t baseScore = static_cast<int32_t>(attempt.velocityKmh * config_.pointsPerKmh);

    // Apply technique multiplier
    float multiplier = 1.0f + (attempt.technique - 1.0f) * 0.5f;
    baseScore = static_cast<int32_t>(baseScore * multiplier);

    // Add bonuses
    if (attempt.velocityKmh >= config_.worldClassVelocity) {
        baseScore += config_.bonusWorldClass;
    } else if (attempt.velocityKmh >= config_.excellentVelocity) {
        baseScore += config_.bonusExcellent;
    }

    return baseScore;
}

void PowerChallenge::render(cv::Mat& frame) {
    switch (state_) {
        case ChallengeState::INSTRUCTIONS:
            renderInstructions(frame);
            break;
        case ChallengeState::COUNTDOWN:
            renderCountdown(frame);
            break;
        case ChallengeState::ACTIVE:
            renderPowerMeter(frame);
            renderAttemptHistory(frame);
            renderCurrentAttempt(frame);
            renderKickAnimation(frame);
            break;
        case ChallengeState::COMPLETE:
            renderResults(frame);
            break;
        default:
            break;
    }
}

void PowerChallenge::renderPowerMeter(cv::Mat& frame) {
    int meterWidth = 60;
    int meterHeight = 400;
    int meterX = frame.cols - meterWidth - 50;
    int meterY = frame.rows / 2 - meterHeight / 2;

    // Background
    cv::rectangle(frame, cv::Point(meterX, meterY),
                 cv::Point(meterX + meterWidth, meterY + meterHeight),
                 cv::Scalar(50, 50, 50), -1);

    // Current velocity (if kicking)
    if (footPositions_.size() >= 5) {
        float currentVelocity = calculateLegVelocity(
            k4abt_skeleton_t()  // Would need actual skeleton
        ) * 3.6f;  // Convert to km/h

        float fillRatio = std::min(1.0f, currentVelocity / config_.worldClassVelocity);
        int fillHeight = static_cast<int>(meterHeight * fillRatio);

        cv::Scalar color = currentVelocity >= config_.worldClassVelocity
            ? cv::Scalar(0, 0, 255)   // Red
            : currentVelocity >= config_.excellentVelocity
            ? cv::Scalar(0, 255, 255)  // Yellow
            : cv::Scalar(0, 255, 0);   // Green

        cv::rectangle(frame,
                     cv::Point(meterX, meterY + meterHeight - fillHeight),
                     cv::Point(meterX + meterWidth, meterY + meterHeight),
                     color, -1);
    }

    // Threshold markers
    auto drawThreshold = [&](float velocity, const std::string& label) {
        float ratio = velocity / config_.worldClassVelocity;
        int y = meterY + meterHeight - static_cast<int>(meterHeight * ratio);

        cv::line(frame, cv::Point(meterX - 5, y),
                cv::Point(meterX + meterWidth + 5, y),
                cv::Scalar(255, 255, 255), 2);

        cv::putText(frame, label, cv::Point(meterX + meterWidth + 15, y + 5),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    };

    drawThreshold(config_.minimumVelocity, "MIN");
    drawThreshold(config_.goodVelocity, "GOOD");
    drawThreshold(config_.excellentVelocity, "EXCELLENT");
    drawThreshold(config_.worldClassVelocity, "WORLD CLASS");
}

void PowerChallenge::renderAttemptHistory(cv::Mat& frame) {
    int yPos = 50;

    // Title
    std::string title = "Attempts: " + std::to_string(attempts_.size()) +
                       "/" + std::to_string(config_.maxAttempts);
    cv::putText(frame, title, cv::Point(50, yPos),
               cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar(255, 255, 255), 2);
    yPos += 60;

    // List attempts
    for (size_t i = 0; i < attempts_.size(); i++) {
        const auto& attempt = attempts_[i];

        std::string attemptText = "#" + std::to_string(i + 1) + ": " +
                                 std::to_string(static_cast<int>(attempt.velocityKmh)) +
                                 " km/h - " + attempt.rating;

        cv::Scalar color = attempt.velocityKmh >= config_.excellentVelocity
            ? cv::Scalar(0, 255, 0)
            : cv::Scalar(255, 255, 255);

        cv::putText(frame, attemptText, cv::Point(50, yPos),
                   cv::FONT_HERSHEY_SIMPLEX, 1.0, color, 2);
        yPos += 50;
    }

    // Personal best
    yPos += 20;
    std::string pbText = "Personal Best: " +
                        std::to_string(static_cast<int>(personalBest_)) + " km/h";
    cv::putText(frame, pbText, cv::Point(50, yPos),
               cv::FONT_HERSHEY_BOLD, 1.3, cv::Scalar(0, 255, 255), 3);
}

void PowerChallenge::renderCurrentAttempt(cv::Mat& frame) {
    std::string stateText;
    cv::Scalar color(255, 255, 255);

    switch (kickState_) {
        case PowerKickState::WAITING:
            stateText = "Ready to kick...";
            break;
        case PowerKickState::WINDUP:
            stateText = "WIND UP!";
            color = cv::Scalar(0, 255, 255);
            break;
        case PowerKickState::IMPACT:
            stateText = "KICK!";
            color = cv::Scalar(0, 255, 0);
            break;
        case PowerKickState::COOLDOWN:
            stateText = "Get ready for next attempt";
            break;
    }

    cv::Size textSize = cv::getTextSize(stateText, cv::FONT_HERSHEY_BOLD,
                                       1.5, 3, nullptr);
    cv::Point pos(frame.cols / 2 - textSize.width / 2, frame.rows - 150);
    cv::putText(frame, stateText, pos, cv::FONT_HERSHEY_BOLD,
               1.5, color, 3);
}

void PowerChallenge::renderKickAnimation(cv::Mat& frame) {
    if (kickAnimationProgress_ <= 0.0f) {
        return;
    }

    // Show velocity achieved
    std::string velocityText = std::to_string(static_cast<int>(lastKickVelocity_)) + " KM/H!";

    cv::Size textSize = cv::getTextSize(velocityText, cv::FONT_HERSHEY_BOLD,
                                       3.0, 5, nullptr);

    // Animate upward and fade
    int yOffset = static_cast<int>((1.0f - kickAnimationProgress_) * 200);
    float alpha = kickAnimationProgress_;

    cv::Point pos(frame.cols / 2 - textSize.width / 2,
                 frame.rows / 2 - yOffset);

    cv::putText(frame, velocityText, pos, cv::FONT_HERSHEY_BOLD,
               3.0 * kickAnimationProgress_,
               cv::Scalar(0, 255, 255), static_cast<int>(5 * alpha));
}

} // namespace game
} // namespace kinect

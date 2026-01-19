#pragma once

#include "../../include/GameConfig.h"
#include <k4a/k4a.h>
#include <k4abt.h>
#include <cstdint>
#include <chrono>
#include <string>
#include <opencv2/opencv.hpp>

namespace kinect {
namespace game {

// Challenge result
struct ChallengeResult {
    ChallengeType type;
    int32_t finalScore = 0;
    int32_t attempts = 0;
    int32_t successes = 0;
    float accuracy = 0.0f;  // 0-1
    float maxVelocity = 0.0f;  // km/h
    float avgVelocity = 0.0f;
    float duration = 0.0f;  // seconds
    bool passed = false;
    std::vector<std::string> achievementsUnlocked;
    std::string grade;  // S, A, B, C, D, F
};

// Base class for all challenges
class ChallengeBase {
public:
    explicit ChallengeBase(ChallengeType type);
    virtual ~ChallengeBase() = default;

    // Challenge lifecycle
    virtual void start();
    virtual void processFrame(const k4abt_skeleton_t& skeleton,
                             const k4a_image_t& depthImage,
                             float deltaTime);
    virtual void finish();
    virtual void reset();

    // State management
    ChallengeState getState() const { return state_; }
    bool isActive() const { return state_ == ChallengeState::ACTIVE; }
    bool isComplete() const { return state_ == ChallengeState::COMPLETE; }

    // Results
    ChallengeResult getResult() const { return result_; }

    // Rendering
    virtual void render(cv::Mat& frame) = 0;
    virtual void renderInstructions(cv::Mat& frame);
    virtual void renderCountdown(cv::Mat& frame);
    virtual void renderResults(cv::Mat& frame);

    // Configuration
    ChallengeType getType() const { return type_; }
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

protected:
    // State transitions
    void setState(ChallengeState newState);

    // Timer helpers
    void startTimer();
    void stopTimer();
    float getElapsedTime() const;
    float getRemainingTime(float totalTime) const;

    // Scoring helpers
    void addScore(int32_t points);
    void recordAttempt(bool success);
    void updateResult();

    // Helper to calculate grade
    std::string calculateGrade(int32_t score, int32_t maxScore) const;

    // Members
    ChallengeType type_;
    ChallengeState state_;
    ChallengeResult result_;

    // Timing
    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint startTime_;
    TimePoint currentTime_;
    bool timerRunning_;

    // Countdown
    float countdownRemaining_;

    // Stats
    int32_t currentScore_;
    int32_t totalAttempts_;
    int32_t successfulAttempts_;

private:
    // Non-copyable
    ChallengeBase(const ChallengeBase&) = delete;
    ChallengeBase& operator=(const ChallengeBase&) = delete;
};

} // namespace game
} // namespace kinect

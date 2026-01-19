#include "ChallengeBase.h"
#include <algorithm>

namespace kinect {
namespace game {

ChallengeBase::ChallengeBase(ChallengeType type)
    : type_(type)
    , state_(ChallengeState::IDLE)
    , timerRunning_(false)
    , countdownRemaining_(3.0f)
    , currentScore_(0)
    , totalAttempts_(0)
    , successfulAttempts_(0)
{
    result_.type = type;
}

void ChallengeBase::start() {
    setState(ChallengeState::INSTRUCTIONS);
    reset();
}

void ChallengeBase::processFrame(const k4abt_skeleton_t& skeleton,
                                 const k4a_image_t& depthImage,
                                 float deltaTime)
{
    currentTime_ = std::chrono::steady_clock::now();

    // Handle countdown
    if (state_ == ChallengeState::COUNTDOWN) {
        countdownRemaining_ -= deltaTime;
        if (countdownRemaining_ <= 0.0f) {
            setState(ChallengeState::ACTIVE);
            startTimer();
        }
    }
}

void ChallengeBase::finish() {
    stopTimer();
    updateResult();
    setState(ChallengeState::COMPLETE);
}

void ChallengeBase::reset() {
    state_ = ChallengeState::IDLE;
    currentScore_ = 0;
    totalAttempts_ = 0;
    successfulAttempts_ = 0;
    countdownRemaining_ = 3.0f;
    timerRunning_ = false;

    result_ = ChallengeResult();
    result_.type = type_;
}

void ChallengeBase::setState(ChallengeState newState) {
    state_ = newState;
}

void ChallengeBase::startTimer() {
    startTime_ = std::chrono::steady_clock::now();
    timerRunning_ = true;
}

void ChallengeBase::stopTimer() {
    timerRunning_ = false;
}

float ChallengeBase::getElapsedTime() const {
    if (!timerRunning_) {
        return 0.0f;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        currentTime_ - startTime_
    );
    return duration.count() / 1000.0f;
}

float ChallengeBase::getRemainingTime(float totalTime) const {
    float elapsed = getElapsedTime();
    return std::max(0.0f, totalTime - elapsed);
}

void ChallengeBase::addScore(int32_t points) {
    currentScore_ += points;
}

void ChallengeBase::recordAttempt(bool success) {
    totalAttempts_++;
    if (success) {
        successfulAttempts_++;
    }
}

void ChallengeBase::updateResult() {
    result_.finalScore = currentScore_;
    result_.attempts = totalAttempts_;
    result_.successes = successfulAttempts_;
    result_.accuracy = totalAttempts_ > 0
        ? static_cast<float>(successfulAttempts_) / totalAttempts_
        : 0.0f;
    result_.duration = getElapsedTime();
}

std::string ChallengeBase::calculateGrade(int32_t score, int32_t maxScore) const {
    if (maxScore == 0) return "F";

    float ratio = static_cast<float>(score) / maxScore;

    if (ratio >= 0.95f) return "S";  // Perfect or near-perfect
    if (ratio >= 0.85f) return "A";
    if (ratio >= 0.70f) return "B";
    if (ratio >= 0.55f) return "C";
    if (ratio >= 0.40f) return "D";
    return "F";
}

void ChallengeBase::renderInstructions(cv::Mat& frame) {
    // Draw semi-transparent overlay
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Point(0, 0),
                 cv::Point(frame.cols, frame.rows),
                 cv::Scalar(0, 0, 0), -1);
    cv::addWeighted(overlay, 0.7, frame, 0.3, 0, frame);

    // Title
    std::string title = getName();
    cv::Size textSize = cv::getTextSize(title, cv::FONT_HERSHEY_BOLD,
                                       2.5, 4, nullptr);
    cv::Point titlePos(frame.cols / 2 - textSize.width / 2, 200);
    cv::putText(frame, title, titlePos, cv::FONT_HERSHEY_BOLD,
               2.5, cv::Scalar(0, 255, 255), 4);

    // Description
    std::string desc = getDescription();
    textSize = cv::getTextSize(desc, cv::FONT_HERSHEY_SIMPLEX,
                              1.2, 2, nullptr);
    cv::Point descPos(frame.cols / 2 - textSize.width / 2, 300);
    cv::putText(frame, desc, descPos, cv::FONT_HERSHEY_SIMPLEX,
               1.2, cv::Scalar(255, 255, 255), 2);

    // Ready prompt
    std::string ready = "Wave to start!";
    textSize = cv::getTextSize(ready, cv::FONT_HERSHEY_BOLD,
                              1.5, 3, nullptr);
    cv::Point readyPos(frame.cols / 2 - textSize.width / 2,
                      frame.rows - 150);
    cv::putText(frame, ready, readyPos, cv::FONT_HERSHEY_BOLD,
               1.5, cv::Scalar(0, 255, 0), 3);
}

void ChallengeBase::renderCountdown(cv::Mat& frame) {
    int countdown = static_cast<int>(std::ceil(countdownRemaining_));
    if (countdown < 1) countdown = 1;

    std::string countText = std::to_string(countdown);
    cv::Size textSize = cv::getTextSize(countText, cv::FONT_HERSHEY_BOLD,
                                       10.0, 15, nullptr);
    cv::Point pos(frame.cols / 2 - textSize.width / 2,
                 frame.rows / 2 + textSize.height / 2);

    // Pulsing effect
    float pulse = 1.0f + (1.0f - (countdownRemaining_ - countdown)) * 0.3f;
    cv::Scalar color = countdown <= 1
        ? cv::Scalar(0, 255, 0)  // Green for GO
        : cv::Scalar(0, 255, 255);  // Yellow for countdown

    cv::putText(frame, countText, pos, cv::FONT_HERSHEY_BOLD,
               10.0 * pulse, color, static_cast<int>(15 * pulse));
}

void ChallengeBase::renderResults(cv::Mat& frame) {
    // Background
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, cv::Point(0, 0),
                 cv::Point(frame.cols, frame.rows),
                 cv::Scalar(20, 20, 20), -1);
    cv::addWeighted(overlay, 0.85, frame, 0.15, 0, frame);

    int yPos = 150;

    // Title
    std::string title = "Challenge Complete!";
    cv::Size textSize = cv::getTextSize(title, cv::FONT_HERSHEY_BOLD,
                                       2.0, 3, nullptr);
    cv::Point titlePos(frame.cols / 2 - textSize.width / 2, yPos);
    cv::putText(frame, title, titlePos, cv::FONT_HERSHEY_BOLD,
               2.0, cv::Scalar(0, 255, 255), 3);

    yPos += 100;

    // Grade
    result_.grade = calculateGrade(result_.finalScore, 1000);  // Subclass should override
    textSize = cv::getTextSize(result_.grade, cv::FONT_HERSHEY_BOLD,
                              8.0, 12, nullptr);
    cv::Point gradePos(frame.cols / 2 - textSize.width / 2, yPos + textSize.height);

    cv::Scalar gradeColor = result_.grade == "S" || result_.grade == "A"
        ? cv::Scalar(0, 255, 0)  // Green
        : result_.grade == "B" || result_.grade == "C"
        ? cv::Scalar(0, 255, 255)  // Yellow
        : cv::Scalar(0, 0, 255);  // Red

    cv::putText(frame, result_.grade, gradePos, cv::FONT_HERSHEY_BOLD,
               8.0, gradeColor, 12);

    yPos += 250;

    // Stats
    auto drawStat = [&](const std::string& label, const std::string& value) {
        std::string stat = label + ": " + value;
        cv::putText(frame, stat, cv::Point(frame.cols / 2 - 300, yPos),
                   cv::FONT_HERSHEY_SIMPLEX, 1.3, cv::Scalar(255, 255, 255), 2);
        yPos += 60;
    };

    drawStat("Score", std::to_string(result_.finalScore));
    drawStat("Accuracy", std::to_string(static_cast<int>(result_.accuracy * 100)) + "%");
    drawStat("Attempts", std::to_string(result_.attempts));
    drawStat("Time", std::to_string(static_cast<int>(result_.duration)) + "s");
}

} // namespace game
} // namespace kinect

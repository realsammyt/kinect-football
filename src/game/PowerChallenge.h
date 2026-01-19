#pragma once

#include "ChallengeBase.h"
#include "../../include/GameConfig.h"
#include <deque>

namespace kinect {
namespace game {

// Power kick attempt
struct PowerKickAttempt {
    float velocity;        // m/s
    float velocityKmh;    // km/h
    float legSpeed;       // m/s
    float technique;      // 0-1 score
    int32_t score;
    std::string rating;   // "Weak", "Good", "Excellent", "World Class"
    uint64_t timestamp;
};

class PowerChallenge : public ChallengeBase {
public:
    explicit PowerChallenge(const PowerChallengeConfig& config);
    virtual ~PowerChallenge() = default;

    // Override base methods
    void start() override;
    void processFrame(const k4abt_skeleton_t& skeleton,
                     const k4a_image_t& depthImage,
                     float deltaTime) override;
    void finish() override;
    void reset() override;

    void render(cv::Mat& frame) override;

    std::string getName() const override { return "Power Challenge"; }
    std::string getDescription() const override {
        return "Kick as hard as you can! 3 attempts to show maximum power";
    }

    // Power-specific
    const std::vector<PowerKickAttempt>& getAttempts() const { return attempts_; }
    float getPersonalBest() const { return personalBest_; }
    void setPersonalBest(float velocity) { personalBest_ = velocity; }

private:
    // Kick detection
    void detectPowerKick(const k4abt_skeleton_t& skeleton, float deltaTime);
    float calculateLegVelocity(const k4abt_skeleton_t& skeleton);
    float calculateTechnique(const k4abt_skeleton_t& skeleton);
    std::string getRating(float velocityKmh);

    // Scoring
    void recordPowerKick(const PowerKickAttempt& attempt);
    int32_t calculatePowerScore(const PowerKickAttempt& attempt);

    // Rendering helpers
    void renderPowerMeter(cv::Mat& frame);
    void renderAttemptHistory(cv::Mat& frame);
    void renderCurrentAttempt(cv::Mat& frame);
    void renderKickAnimation(cv::Mat& frame);

    // Configuration
    PowerChallengeConfig config_;

    // Attempts
    std::vector<PowerKickAttempt> attempts_;
    float personalBest_;

    // Kick detection
    enum class PowerKickState {
        WAITING,
        WINDUP,
        IMPACT,
        COOLDOWN
    };
    PowerKickState kickState_;
    float kickTimer_;

    // Velocity tracking
    std::deque<k4a_float3_t> footPositions_;
    std::deque<float> footTimestamps_;
    static const size_t MAX_FOOT_HISTORY = 10;

    // Animation
    float kickAnimationProgress_;
    float lastKickVelocity_;
};

} // namespace game
} // namespace kinect

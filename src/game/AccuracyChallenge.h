#pragma once

#include "ChallengeBase.h"
#include "../../include/GameConfig.h"

namespace kinect {
namespace game {

// Kick data for accuracy tracking
struct KickData {
    k4a_float3_t impactPoint;  // Where ball hit goal
    TargetZone::Position targetZone;
    float velocity;  // m/s
    bool onTarget;
    float accuracy;  // 0-1, distance from target center
    uint64_t timestamp;
};

class AccuracyChallenge : public ChallengeBase {
public:
    explicit AccuracyChallenge(const AccuracyChallengeConfig& config);
    virtual ~AccuracyChallenge() = default;

    // Override base methods
    void start() override;
    void processFrame(const k4abt_skeleton_t& skeleton,
                     const k4a_image_t& depthImage,
                     float deltaTime) override;
    void finish() override;
    void reset() override;

    void render(cv::Mat& frame) override;

    std::string getName() const override { return "Accuracy Challenge"; }
    std::string getDescription() const override {
        return "Hit all 9 target zones! Corners = 3x points, Edges = 2x";
    }

    // Accuracy-specific
    void setActiveTarget(TargetZone::Position target);
    TargetZone::Position getActiveTarget() const { return activeTarget_; }
    const std::vector<TargetZone>& getTargetZones() const { return targetZones_; }
    const std::vector<KickData>& getKickHistory() const { return kickHistory_; }

private:
    // Kick detection
    void detectKick(const k4abt_skeleton_t& skeleton, float deltaTime);
    bool isKickingPose(const k4abt_skeleton_t& skeleton);
    k4a_float3_t estimateBallTrajectory(const k4abt_skeleton_t& skeleton);
    TargetZone::Position determineHitZone(const k4a_float3_t& impactPoint);

    // Scoring
    void recordKick(const KickData& kick);
    int32_t calculateKickScore(const KickData& kick);
    void checkComboBonus();

    // Rendering helpers
    void renderTargetGrid(cv::Mat& frame);
    void renderActiveTarget(cv::Mat& frame);
    void renderStats(cv::Mat& frame);
    void renderKickTrajectory(cv::Mat& frame);

    // Configuration
    AccuracyChallengeConfig config_;

    // Target zones
    std::vector<TargetZone> targetZones_;
    TargetZone::Position activeTarget_;

    // Kick tracking
    std::vector<KickData> kickHistory_;
    int32_t consecutiveHits_;
    float lastKickTime_;

    // Kick detection state
    enum class KickState {
        IDLE,
        WINDING_UP,
        KICKING,
        FOLLOW_THROUGH
    };
    KickState kickState_;
    k4a_float3_t lastFootPosition_;
    float kickPhaseTimer_;
};

} // namespace game
} // namespace kinect

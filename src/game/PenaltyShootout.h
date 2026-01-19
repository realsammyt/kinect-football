#pragma once

#include "ChallengeBase.h"
#include "../../include/GameConfig.h"
#include <random>

namespace kinect {
namespace game {

// Penalty kick result
struct PenaltyKick {
    enum class Result {
        GOAL,
        SAVED,
        MISSED,
        PENDING
    };

    k4a_float3_t kickDirection;
    k4a_float3_t targetPoint;
    TargetZone::Position targetZone;
    float velocity;  // m/s
    Result result;
    int32_t score;
    uint64_t timestamp;
};

// Goalkeeper AI
class GoalkeeperAI {
public:
    explicit GoalkeeperAI(float reactionTime, float coverage, float randomness);

    // Predict and dive
    TargetZone::Position predictDive(const k4abt_skeleton_t& skeleton,
                                     const k4a_float3_t& kickDirection);
    bool willSave(const TargetZone::Position& kickZone,
                  const TargetZone::Position& diveZone,
                  float velocity);

    void reset();

private:
    float reactionTime_;   // Delay before dive
    float coverage_;       // Percentage of goal covered
    float randomness_;     // Unpredictability factor

    std::mt19937 rng_;
    TargetZone::Position lastDive_;
};

class PenaltyShootout : public ChallengeBase {
public:
    explicit PenaltyShootout(const PenaltyShootoutConfig& config);
    virtual ~PenaltyShootout() = default;

    // Override base methods
    void start() override;
    void processFrame(const k4abt_skeleton_t& skeleton,
                     const k4a_image_t& depthImage,
                     float deltaTime) override;
    void finish() override;
    void reset() override;

    void render(cv::Mat& frame) override;

    std::string getName() const override { return "Penalty Shootout"; }
    std::string getDescription() const override {
        return "Score penalties against the goalkeeper! Best of 5";
    }

    // Penalty-specific
    const std::vector<PenaltyKick>& getKicks() const { return kicks_; }
    int32_t getGoalsScored() const { return goalsScored_; }
    int32_t getCurrentRound() const { return currentRound_; }
    bool isSuddenDeath() const { return suddenDeath_; }

private:
    // Kick detection and execution
    void detectPenaltyKick(const k4abt_skeleton_t& skeleton, float deltaTime);
    k4a_float3_t estimateKickDirection(const k4abt_skeleton_t& skeleton);
    TargetZone::Position determineTargetZone(const k4a_float3_t& direction);
    void executePenalty(const k4abt_skeleton_t& skeleton);

    // Goalkeeper interaction
    void updateGoalkeeper(float deltaTime);
    bool checkSave(const PenaltyKick& kick);

    // Scoring
    void recordPenalty(PenaltyKick kick);
    void advanceRound();

    // Rendering helpers
    void renderGoalkeeper(cv::Mat& frame);
    void renderScoreboard(cv::Mat& frame);
    void renderKickResult(cv::Mat& frame);
    void renderAimingGuide(cv::Mat& frame);

    // Configuration
    PenaltyShootoutConfig config_;

    // Game state
    std::vector<PenaltyKick> kicks_;
    int32_t currentRound_;
    int32_t goalsScored_;
    int32_t goalsMissed_;
    bool suddenDeath_;

    // Goalkeeper
    std::unique_ptr<GoalkeeperAI> goalkeeper_;
    TargetZone::Position goalkeeperDive_;
    float goalkeeperAnimationTime_;

    // Kick state machine
    enum class PenaltyState {
        POSITIONING,   // Player getting ready
        AIMING,        // Showing aim guide
        WINDUP,        // Winding up for kick
        KICKED,        // Ball in flight
        RESULT_SHOW,   // Showing result
        NEXT_ROUND     // Transitioning to next
    };
    PenaltyState penaltyState_;
    float stateTimer_;

    // Kick detection
    k4a_float3_t lastFootPosition_;
    std::vector<k4a_float3_t> footTrajectory_;

    // Result animation
    PenaltyKick::Result lastResult_;
    float resultAnimationTime_;
};

} // namespace game
} // namespace kinect

#ifndef KINECT_FOOTBALL_KICK_ANALYZER_H
#define KINECT_FOOTBALL_KICK_ANALYZER_H

#include "MotionHistory.h"
#include "../../include/KickTypes.h"
#include <k4abt.h>

namespace kinect {
namespace motion {

// Target zone for accuracy calculation
struct TargetZone {
    k4a_float3_t center;
    float radius; // meters

    TargetZone() : center{0.0f, 0.0f, 3.0f}, radius(0.5f) {}
    TargetZone(const k4a_float3_t& c, float r) : center(c), radius(r) {}
};

class KickAnalyzer {
public:
    // Score weights for overall calculation
    static constexpr float POWER_WEIGHT = 0.30f;
    static constexpr float ACCURACY_WEIGHT = 0.25f;
    static constexpr float TECHNIQUE_WEIGHT = 0.25f;
    static constexpr float BALANCE_WEIGHT = 0.20f;

    // Reference values for scoring
    static constexpr float MAX_BALL_SPEED_KMH = 120.0f; // Professional level
    static constexpr float IDEAL_KNEE_ANGLE = 135.0f;   // Degrees
    static constexpr float MAX_HIP_ROTATION = 90.0f;    // Degrees

    KickAnalyzer();
    ~KickAnalyzer() = default;

    // Analyze a completed kick with full skeleton and motion data
    KickResult analyzeKick(
        const k4abt_skeleton_t& skeleton,
        const MotionHistory& ankleHistory,
        const MotionHistory& footHistory,
        const MotionHistory& kneeHistory,
        const MotionHistory& hipHistory,
        const MotionHistory& pelvisHistory,
        DominantFoot foot,
        uint64_t timestamp
    );

    // Set target zone for accuracy calculation
    void setTargetZone(const TargetZone& target) { targetZone_ = target; }

    // Classify kick type based on motion pattern
    KickType classifyKickType(
        const k4abt_skeleton_t& skeleton,
        const MotionHistory& footHistory,
        const MotionHistory& kneeHistory,
        DominantFoot foot
    );

private:
    TargetZone targetZone_;

    // Power analysis
    float calculatePower(const MotionHistory& footHistory);
    float calculateEstimatedBallSpeed(float footVelocity);
    float calculatePowerScore(float ballSpeed);

    // Accuracy analysis
    float calculateDirectionAngle(const k4a_float3_t& kickDirection);
    float calculateAccuracyScore(float directionAngle);

    // Technique analysis
    float calculateKneeAngle(const k4abt_skeleton_t& skeleton, DominantFoot foot);
    float calculateHipRotation(const k4abt_skeleton_t& skeleton, DominantFoot foot);
    float calculateFollowThroughLength(const MotionHistory& footHistory);
    float calculateTechniqueScore(float kneeAngle, float hipRotation, float followThrough);

    // Balance analysis
    float calculateBodyLean(const k4abt_skeleton_t& skeleton);
    float calculateBalanceScore(float bodyLean);

    // Overall score
    float calculateOverallScore(const KickQuality& quality);

    // Helper: calculate angle between three joints
    static float calculateJointAngle(const k4a_float3_t& joint1,
                                     const k4a_float3_t& joint2,
                                     const k4a_float3_t& joint3);

    // Helper: vector operations
    static float magnitude(const k4a_float3_t& v);
    static k4a_float3_t normalize(const k4a_float3_t& v);
    static k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b);
    static float dotProduct(const k4a_float3_t& a, const k4a_float3_t& b);
    static k4a_float3_t crossProduct(const k4a_float3_t& a, const k4a_float3_t& b);

    // Helper: angle between vectors in degrees
    static float angleBetweenVectors(const k4a_float3_t& a, const k4a_float3_t& b);
};

} // namespace motion
} // namespace kinect

#endif // KINECT_FOOTBALL_KICK_ANALYZER_H

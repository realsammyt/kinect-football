#ifndef KINECT_FOOTBALL_KICK_DETECTOR_H
#define KINECT_FOOTBALL_KICK_DETECTOR_H

#include "MotionHistory.h"
#include "../../include/KickTypes.h"
#include <k4abt.h>
#include <functional>
#include <memory>

namespace kinect {
namespace motion {

// Callback when kick is completed
using KickCallback = std::function<void(const KickResult&)>;

class KickDetector {
public:
    // Velocity thresholds for phase detection (m/s)
    static constexpr float VELOCITY_WINDUP = 0.5f;
    static constexpr float VELOCITY_ACCELERATION = 2.0f;
    static constexpr float VELOCITY_IDLE = 0.3f;

    // Minimum time in each phase (microseconds)
    static constexpr uint64_t MIN_WINDUP_TIME = 200000;      // 0.2s
    static constexpr uint64_t MIN_ACCELERATION_TIME = 100000; // 0.1s

    KickDetector();
    ~KickDetector() = default;

    // Process new skeleton frame
    void processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp);

    // Set callback for kick completion
    void setKickCallback(KickCallback callback) { kickCallback_ = callback; }

    // Get current phase
    KickPhase getCurrentPhase() const { return currentPhase_; }

    // Get dominant foot being used
    DominantFoot getDominantFoot() const { return dominantFoot_; }

    // Reset detector state
    void reset();

private:
    // Motion history for key joints
    MotionHistory leftAnkleHistory_;
    MotionHistory rightAnkleHistory_;
    MotionHistory leftFootHistory_;
    MotionHistory rightFootHistory_;
    MotionHistory leftKneeHistory_;
    MotionHistory rightKneeHistory_;
    MotionHistory leftHipHistory_;
    MotionHistory rightHipHistory_;
    MotionHistory pelvisHistory_;

    // State tracking
    KickPhase currentPhase_;
    DominantFoot dominantFoot_;
    uint64_t phaseStartTime_;
    float peakVelocity_;
    k4a_float3_t kickDirection_;

    // Callback
    KickCallback kickCallback_;

    // Current skeleton (stored for analysis)
    k4abt_skeleton_t currentSkeleton_;
    uint64_t currentTimestamp_;

    // Phase detection methods
    void updatePhase(uint64_t timestamp);
    bool detectWindUp(const MotionHistory& ankleHistory, const MotionHistory& footHistory);
    bool detectAcceleration(const MotionHistory& ankleHistory, const MotionHistory& footHistory);
    bool detectContact(const MotionHistory& ankleHistory, const MotionHistory& footHistory);
    bool detectFollowThrough(const MotionHistory& ankleHistory, const MotionHistory& footHistory);

    // Determine which foot is kicking
    void updateDominantFoot();

    // Calculate kick direction vector
    k4a_float3_t calculateKickDirection() const;

    // Get the appropriate history for current dominant foot
    MotionHistory& getActiveAnkleHistory();
    MotionHistory& getActiveFootHistory();
    MotionHistory& getActiveKneeHistory();
    MotionHistory& getActiveHipHistory();

    const MotionHistory& getActiveAnkleHistory() const;
    const MotionHistory& getActiveFootHistory() const;

    // Complete kick and trigger callback
    void completeKick();

    // Helper: calculate angle between three joints
    static float calculateJointAngle(const k4a_float3_t& joint1,
                                     const k4a_float3_t& joint2,
                                     const k4a_float3_t& joint3);

    // Helper: vector operations
    static float magnitude(const k4a_float3_t& v);
    static k4a_float3_t normalize(const k4a_float3_t& v);
    static k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b);
    static float dotProduct(const k4a_float3_t& a, const k4a_float3_t& b);
};

} // namespace motion
} // namespace kinect

#endif // KINECT_FOOTBALL_KICK_DETECTOR_H

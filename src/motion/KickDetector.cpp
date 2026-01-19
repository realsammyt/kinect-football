#include "KickDetector.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace motion {

KickDetector::KickDetector()
    : currentPhase_(KickPhase::Idle)
    , dominantFoot_(DominantFoot::Unknown)
    , phaseStartTime_(0)
    , peakVelocity_(0.0f)
    , kickDirection_{0.0f, 0.0f, 0.0f}
    , currentTimestamp_(0)
{
    currentSkeleton_ = {};
}

void KickDetector::processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp) {
    currentSkeleton_ = skeleton;
    currentTimestamp_ = timestamp;

    // Update motion histories for all relevant joints
    leftAnkleHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_ANKLE_LEFT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_ANKLE_LEFT].confidence_level
    );

    rightAnkleHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_ANKLE_RIGHT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_ANKLE_RIGHT].confidence_level
    );

    leftFootHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_FOOT_LEFT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_FOOT_LEFT].confidence_level
    );

    rightFootHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_FOOT_RIGHT].confidence_level
    );

    leftKneeHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_KNEE_LEFT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_KNEE_LEFT].confidence_level
    );

    rightKneeHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_KNEE_RIGHT].confidence_level
    );

    leftHipHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_HIP_LEFT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_HIP_LEFT].confidence_level
    );

    rightHipHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_HIP_RIGHT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_HIP_RIGHT].confidence_level
    );

    pelvisHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_PELVIS].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_PELVIS].confidence_level
    );

    // Update phase state machine
    updatePhase(timestamp);
}

void KickDetector::updatePhase(uint64_t timestamp) {
    // Determine which foot is more active
    updateDominantFoot();

    if (dominantFoot_ == DominantFoot::Unknown) {
        return; // Can't detect kicks without knowing which foot
    }

    auto& ankleHistory = getActiveAnkleHistory();
    auto& footHistory = getActiveFootHistory();

    switch (currentPhase_) {
        case KickPhase::Idle:
            if (detectWindUp(ankleHistory, footHistory)) {
                currentPhase_ = KickPhase::WindUp;
                phaseStartTime_ = timestamp;
                peakVelocity_ = 0.0f;
            }
            break;

        case KickPhase::WindUp:
            // Check minimum time in phase
            if (timestamp - phaseStartTime_ >= MIN_WINDUP_TIME) {
                if (detectAcceleration(ankleHistory, footHistory)) {
                    currentPhase_ = KickPhase::Acceleration;
                    phaseStartTime_ = timestamp;
                }
            }
            // Timeout back to idle if wind-up takes too long
            if (timestamp - phaseStartTime_ > 2000000) { // 2 seconds
                reset();
            }
            break;

        case KickPhase::Acceleration: {
            // Track peak velocity
            float currentSpeed = footHistory.getCurrentSpeed();
            if (currentSpeed > peakVelocity_) {
                peakVelocity_ = currentSpeed;
            }

            // Check minimum time in phase
            if (timestamp - phaseStartTime_ >= MIN_ACCELERATION_TIME) {
                if (detectContact(ankleHistory, footHistory)) {
                    currentPhase_ = KickPhase::Contact;
                    phaseStartTime_ = timestamp;
                    kickDirection_ = calculateKickDirection();
                }
            }
            break;
        }

        case KickPhase::Contact:
            // Contact is brief, quickly move to follow-through
            if (detectFollowThrough(ankleHistory, footHistory)) {
                currentPhase_ = KickPhase::FollowThrough;
                phaseStartTime_ = timestamp;
            }
            break;

        case KickPhase::FollowThrough:
            // Complete kick after sufficient follow-through
            if (timestamp - phaseStartTime_ > 300000) { // 0.3 seconds
                completeKick();
                reset();
            }
            break;
    }
}

bool KickDetector::detectWindUp(const MotionHistory& ankleHistory, const MotionHistory& footHistory) {
    if (!ankleHistory.hasEnoughData()) {
        return false;
    }

    float speed = ankleHistory.getCurrentSpeed();
    k4a_float3_t velocity = ankleHistory.getCurrentVelocity();

    // Wind-up is backward motion (negative Z in Kinect coordinate system)
    return speed > VELOCITY_WINDUP && velocity.xyz.z < 0;
}

bool KickDetector::detectAcceleration(const MotionHistory& ankleHistory, const MotionHistory& footHistory) {
    if (!footHistory.hasEnoughData()) {
        return false;
    }

    float speed = footHistory.getCurrentSpeed();
    k4a_float3_t velocity = footHistory.getCurrentVelocity();

    // Acceleration is forward motion (positive Z) with high velocity
    return speed > VELOCITY_ACCELERATION && velocity.xyz.z > 0;
}

bool KickDetector::detectContact(const MotionHistory& ankleHistory, const MotionHistory& footHistory) {
    if (!footHistory.hasEnoughData()) {
        return false;
    }

    // Contact detected by sudden deceleration after peak velocity
    float currentSpeed = footHistory.getCurrentSpeed();
    float previousSpeed = 0.0f;

    k4a_float3_t prevVel;
    if (footHistory.getVelocity(1, prevVel)) {
        previousSpeed = magnitude(prevVel);
    }

    // Deceleration threshold
    return (previousSpeed > VELOCITY_ACCELERATION * 0.8f) && (currentSpeed < previousSpeed * 0.7f);
}

bool KickDetector::detectFollowThrough(const MotionHistory& ankleHistory, const MotionHistory& footHistory) {
    if (!footHistory.hasEnoughData()) {
        return false;
    }

    float speed = footHistory.getCurrentSpeed();

    // Follow-through is continued forward motion but decelerating
    k4a_float3_t velocity = footHistory.getCurrentVelocity();
    return velocity.xyz.z > 0 && speed < VELOCITY_ACCELERATION;
}

void KickDetector::updateDominantFoot() {
    // Compare foot velocities to determine which foot is kicking
    float leftSpeed = leftFootHistory_.getCurrentSpeed();
    float rightSpeed = rightFootHistory_.getCurrentSpeed();

    // Only change dominant foot if there's a clear difference
    if (leftSpeed > rightSpeed * 1.5f) {
        dominantFoot_ = DominantFoot::Left;
    } else if (rightSpeed > leftSpeed * 1.5f) {
        dominantFoot_ = DominantFoot::Right;
    }
    // Keep current dominantFoot_ if speeds are similar
}

k4a_float3_t KickDetector::calculateKickDirection() const {
    // Direction is based on foot velocity at peak
    auto& footHistory = getActiveFootHistory();
    k4a_float3_t direction = footHistory.getAverageVelocity(3); // Average last 3 frames
    return normalize(direction);
}

MotionHistory& KickDetector::getActiveAnkleHistory() {
    return dominantFoot_ == DominantFoot::Left ? leftAnkleHistory_ : rightAnkleHistory_;
}

MotionHistory& KickDetector::getActiveFootHistory() {
    return dominantFoot_ == DominantFoot::Left ? leftFootHistory_ : rightFootHistory_;
}

MotionHistory& KickDetector::getActiveKneeHistory() {
    return dominantFoot_ == DominantFoot::Left ? leftKneeHistory_ : rightKneeHistory_;
}

MotionHistory& KickDetector::getActiveHipHistory() {
    return dominantFoot_ == DominantFoot::Left ? leftHipHistory_ : rightHipHistory_;
}

const MotionHistory& KickDetector::getActiveAnkleHistory() const {
    return dominantFoot_ == DominantFoot::Left ? leftAnkleHistory_ : rightAnkleHistory_;
}

const MotionHistory& KickDetector::getActiveFootHistory() const {
    return dominantFoot_ == DominantFoot::Left ? leftFootHistory_ : rightFootHistory_;
}

void KickDetector::completeKick() {
    if (!kickCallback_) {
        return;
    }

    KickResult result;
    result.foot = dominantFoot_;
    result.kickDirection = kickDirection_;
    result.timestamp = currentTimestamp_;
    result.isValid = true;

    // Basic quality metrics (will be enhanced by KickAnalyzer)
    result.quality.footVelocity = peakVelocity_;
    result.quality.estimatedBallSpeed = peakVelocity_ * 3.6f; // m/s to km/h

    // Determine kick type based on motion pattern
    result.type = KickType::Instep; // Default, can be refined

    kickCallback_(result);
}

void KickDetector::reset() {
    currentPhase_ = KickPhase::Idle;
    dominantFoot_ = DominantFoot::Unknown;
    phaseStartTime_ = 0;
    peakVelocity_ = 0.0f;
    kickDirection_ = {0.0f, 0.0f, 0.0f};
}

float KickDetector::calculateJointAngle(const k4a_float3_t& joint1,
                                         const k4a_float3_t& joint2,
                                         const k4a_float3_t& joint3) {
    k4a_float3_t v1 = normalize(subtract(joint1, joint2));
    k4a_float3_t v2 = normalize(subtract(joint3, joint2));

    float dot = dotProduct(v1, v2);
    dot = std::max(-1.0f, std::min(1.0f, dot)); // Clamp for numerical stability

    return std::acos(dot) * (180.0f / 3.14159265359f);
}

float KickDetector::magnitude(const k4a_float3_t& v) {
    return std::sqrt(v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z);
}

k4a_float3_t KickDetector::normalize(const k4a_float3_t& v) {
    float mag = magnitude(v);
    if (mag < 0.0001f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {
        v.xyz.x / mag,
        v.xyz.y / mag,
        v.xyz.z / mag
    };
}

k4a_float3_t KickDetector::subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.x - b.xyz.x,
        a.xyz.y - b.xyz.y,
        a.xyz.z - b.xyz.z
    };
}

float KickDetector::dotProduct(const k4a_float3_t& a, const k4a_float3_t& b) {
    return a.xyz.x * b.xyz.x + a.xyz.y * b.xyz.y + a.xyz.z * b.xyz.z;
}

} // namespace motion
} // namespace kinect

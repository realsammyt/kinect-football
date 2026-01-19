#include "KickAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace motion {

KickAnalyzer::KickAnalyzer() {
    targetZone_ = TargetZone();
}

KickResult KickAnalyzer::analyzeKick(
    const k4abt_skeleton_t& skeleton,
    const MotionHistory& ankleHistory,
    const MotionHistory& footHistory,
    const MotionHistory& kneeHistory,
    const MotionHistory& hipHistory,
    const MotionHistory& pelvisHistory,
    DominantFoot foot,
    uint64_t timestamp)
{
    KickResult result;
    result.foot = foot;
    result.timestamp = timestamp;
    result.isValid = true;

    // Calculate kick direction from peak velocity
    result.kickDirection = normalize(footHistory.getAverageVelocity(3));

    // Classify kick type
    result.type = classifyKickType(skeleton, footHistory, kneeHistory, foot);

    // Power analysis
    result.quality.footVelocity = footHistory.getPeakSpeed();
    result.quality.estimatedBallSpeed = calculateEstimatedBallSpeed(result.quality.footVelocity);
    result.quality.powerScore = calculatePowerScore(result.quality.estimatedBallSpeed);

    // Accuracy analysis
    result.quality.directionAngle = calculateDirectionAngle(result.kickDirection);
    result.quality.accuracyScore = calculateAccuracyScore(result.quality.directionAngle);

    // Technique analysis
    result.quality.kneeAngle = calculateKneeAngle(skeleton, foot);
    result.quality.hipRotation = calculateHipRotation(skeleton, foot);
    result.quality.followThroughLength = calculateFollowThroughLength(footHistory);
    result.quality.techniqueScore = calculateTechniqueScore(
        result.quality.kneeAngle,
        result.quality.hipRotation,
        result.quality.followThroughLength
    );

    // Balance analysis
    result.quality.bodyLean = calculateBodyLean(skeleton);
    result.quality.balanceScore = calculateBalanceScore(result.quality.bodyLean);

    // Overall score
    result.quality.overallScore = calculateOverallScore(result.quality);

    return result;
}

KickType KickAnalyzer::classifyKickType(
    const k4abt_skeleton_t& skeleton,
    const MotionHistory& footHistory,
    const MotionHistory& kneeHistory,
    DominantFoot foot)
{
    // Get joint indices based on dominant foot
    int ankleJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_ANKLE_LEFT : K4ABT_JOINT_ANKLE_RIGHT;
    int kneeJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_KNEE_LEFT : K4ABT_JOINT_KNEE_RIGHT;
    int hipJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_HIP_LEFT : K4ABT_JOINT_HIP_RIGHT;

    float kneeAngle = calculateJointAngle(
        skeleton.joints[hipJoint].position,
        skeleton.joints[kneeJoint].position,
        skeleton.joints[ankleJoint].position
    );

    float peakSpeed = footHistory.getPeakSpeed();
    k4a_float3_t velocity = footHistory.getCurrentVelocity();

    // Classification heuristics
    if (kneeAngle > 160.0f && peakSpeed > 3.0f) {
        return KickType::Instep; // Straight leg, high power
    } else if (kneeAngle < 120.0f) {
        return KickType::SideFootPass; // Bent knee, controlled
    } else if (std::abs(velocity.xyz.x) > std::abs(velocity.xyz.z)) {
        return KickType::Outside; // Lateral motion component
    } else if (peakSpeed > 4.0f && kneeAngle < 140.0f) {
        return KickType::Toe; // High speed, moderate knee bend
    }

    return KickType::Instep; // Default
}

float KickAnalyzer::calculatePower(const MotionHistory& footHistory) {
    return footHistory.getPeakSpeed();
}

float KickAnalyzer::calculateEstimatedBallSpeed(float footVelocity) {
    // Empirical coefficient: ball speed is typically 1.2-1.3x foot speed
    // Convert m/s to km/h
    return footVelocity * 1.25f * 3.6f;
}

float KickAnalyzer::calculatePowerScore(float ballSpeed) {
    // Score 0-100 based on ball speed
    float normalized = ballSpeed / MAX_BALL_SPEED_KMH;
    return std::min(100.0f, normalized * 100.0f);
}

float KickAnalyzer::calculateDirectionAngle(const k4a_float3_t& kickDirection) {
    // Calculate angle from target center
    k4a_float3_t toTarget = normalize(subtract(targetZone_.center, k4a_float3_t{0.0f, 0.0f, 0.0f}));
    return angleBetweenVectors(kickDirection, toTarget);
}

float KickAnalyzer::calculateAccuracyScore(float directionAngle) {
    // Perfect accuracy = 0 degrees, score decreases with angle
    // 0° = 100, 15° = 50, 30° = 0
    float score = 100.0f - (directionAngle / 30.0f) * 100.0f;
    return std::max(0.0f, std::min(100.0f, score));
}

float KickAnalyzer::calculateKneeAngle(const k4abt_skeleton_t& skeleton, DominantFoot foot) {
    int ankleJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_ANKLE_LEFT : K4ABT_JOINT_ANKLE_RIGHT;
    int kneeJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_KNEE_LEFT : K4ABT_JOINT_KNEE_RIGHT;
    int hipJoint = (foot == DominantFoot::Left) ? K4ABT_JOINT_HIP_LEFT : K4ABT_JOINT_HIP_RIGHT;

    return calculateJointAngle(
        skeleton.joints[hipJoint].position,
        skeleton.joints[kneeJoint].position,
        skeleton.joints[ankleJoint].position
    );
}

float KickAnalyzer::calculateHipRotation(const k4abt_skeleton_t& skeleton, DominantFoot foot) {
    // Calculate rotation by comparing hip orientation to pelvis forward direction
    k4a_float3_t leftHip = skeleton.joints[K4ABT_JOINT_HIP_LEFT].position;
    k4a_float3_t rightHip = skeleton.joints[K4ABT_JOINT_HIP_RIGHT].position;
    k4a_float3_t pelvis = skeleton.joints[K4ABT_JOINT_PELVIS].position;

    // Hip line vector
    k4a_float3_t hipLine = subtract(rightHip, leftHip);

    // Forward direction (Z-axis)
    k4a_float3_t forward = {0.0f, 0.0f, 1.0f};

    // Calculate angle
    k4a_float3_t hipLineXZ = {hipLine.xyz.x, 0.0f, hipLine.xyz.z};
    float angle = angleBetweenVectors(normalize(hipLineXZ), forward);

    return angle;
}

float KickAnalyzer::calculateFollowThroughLength(const MotionHistory& footHistory) {
    // Calculate total distance traveled during follow-through
    // Use last 10 frames (roughly 0.33 seconds)
    float totalDistance = 0.0f;
    k4a_float3_t prevPos;

    if (!footHistory.getPosition(10, prevPos)) {
        return 0.0f;
    }

    for (size_t i = 9; i > 0; --i) {
        k4a_float3_t pos;
        if (footHistory.getPosition(i, pos)) {
            totalDistance += magnitude(subtract(pos, prevPos));
            prevPos = pos;
        }
    }

    return totalDistance;
}

float KickAnalyzer::calculateTechniqueScore(float kneeAngle, float hipRotation, float followThrough) {
    // Ideal knee angle score
    float kneeScore = 100.0f - std::abs(kneeAngle - IDEAL_KNEE_ANGLE) / IDEAL_KNEE_ANGLE * 100.0f;
    kneeScore = std::max(0.0f, std::min(100.0f, kneeScore));

    // Hip rotation score (more rotation = better)
    float hipScore = (hipRotation / MAX_HIP_ROTATION) * 100.0f;
    hipScore = std::max(0.0f, std::min(100.0f, hipScore));

    // Follow-through score (longer = better, up to 1 meter)
    float followScore = (followThrough / 1.0f) * 100.0f;
    followScore = std::max(0.0f, std::min(100.0f, followScore));

    // Weighted average
    return (kneeScore * 0.4f + hipScore * 0.3f + followScore * 0.3f);
}

float KickAnalyzer::calculateBodyLean(const k4abt_skeleton_t& skeleton) {
    // Calculate lean from vertical using spine joints
    k4a_float3_t pelvis = skeleton.joints[K4ABT_JOINT_PELVIS].position;
    k4a_float3_t spine = skeleton.joints[K4ABT_JOINT_SPINE_CHEST].position;

    k4a_float3_t spineVector = subtract(spine, pelvis);
    k4a_float3_t vertical = {0.0f, 1.0f, 0.0f};

    return angleBetweenVectors(spineVector, vertical);
}

float KickAnalyzer::calculateBalanceScore(float bodyLean) {
    // Ideal lean is 5-15 degrees forward
    float idealLean = 10.0f;
    float deviation = std::abs(bodyLean - idealLean);

    // Score decreases with deviation
    float score = 100.0f - (deviation / 45.0f) * 100.0f; // 45° = max acceptable lean
    return std::max(0.0f, std::min(100.0f, score));
}

float KickAnalyzer::calculateOverallScore(const KickQuality& quality) {
    return quality.powerScore * POWER_WEIGHT +
           quality.accuracyScore * ACCURACY_WEIGHT +
           quality.techniqueScore * TECHNIQUE_WEIGHT +
           quality.balanceScore * BALANCE_WEIGHT;
}

float KickAnalyzer::calculateJointAngle(const k4a_float3_t& joint1,
                                         const k4a_float3_t& joint2,
                                         const k4a_float3_t& joint3) {
    k4a_float3_t v1 = normalize(subtract(joint1, joint2));
    k4a_float3_t v2 = normalize(subtract(joint3, joint2));

    float dot = dotProduct(v1, v2);
    dot = std::max(-1.0f, std::min(1.0f, dot));

    return std::acos(dot) * (180.0f / 3.14159265359f);
}

float KickAnalyzer::magnitude(const k4a_float3_t& v) {
    return std::sqrt(v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z);
}

k4a_float3_t KickAnalyzer::normalize(const k4a_float3_t& v) {
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

k4a_float3_t KickAnalyzer::subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.x - b.xyz.x,
        a.xyz.y - b.xyz.y,
        a.xyz.z - b.xyz.z
    };
}

float KickAnalyzer::dotProduct(const k4a_float3_t& a, const k4a_float3_t& b) {
    return a.xyz.x * b.xyz.x + a.xyz.y * b.xyz.y + a.xyz.z * b.xyz.z;
}

k4a_float3_t KickAnalyzer::crossProduct(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.y * b.xyz.z - a.xyz.z * b.xyz.y,
        a.xyz.z * b.xyz.x - a.xyz.x * b.xyz.z,
        a.xyz.x * b.xyz.y - a.xyz.y * b.xyz.x
    };
}

float KickAnalyzer::angleBetweenVectors(const k4a_float3_t& a, const k4a_float3_t& b) {
    k4a_float3_t normA = normalize(a);
    k4a_float3_t normB = normalize(b);

    float dot = dotProduct(normA, normB);
    dot = std::max(-1.0f, std::min(1.0f, dot));

    return std::acos(dot) * (180.0f / 3.14159265359f);
}

} // namespace motion
} // namespace kinect

#include "HeaderDetector.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace motion {

HeaderDetector::HeaderDetector()
    : currentPhase_(HeaderPhase::Idle)
    , phaseStartTime_(0)
    , peakHeadVelocity_(0.0f)
    , headerDirection_{0.0f, 0.0f, 0.0f}
    , currentTimestamp_(0)
{
    currentSkeleton_ = {};
}

void HeaderDetector::processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp) {
    currentSkeleton_ = skeleton;
    currentTimestamp_ = timestamp;

    // Update motion histories for relevant joints
    headHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_HEAD].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_HEAD].confidence_level
    );

    neckHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_NECK].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_NECK].confidence_level
    );

    spineChestHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_SPINE_CHEST].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_SPINE_CHEST].confidence_level
    );

    pelvisHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_PELVIS].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_PELVIS].confidence_level
    );

    shoulderLeftHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_SHOULDER_LEFT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_SHOULDER_LEFT].confidence_level
    );

    shoulderRightHistory_.addFrame(
        skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].position,
        timestamp,
        skeleton.joints[K4ABT_JOINT_SHOULDER_RIGHT].confidence_level
    );

    // Update phase state machine
    updatePhase(timestamp);
}

void HeaderDetector::updatePhase(uint64_t timestamp) {
    switch (currentPhase_) {
        case HeaderPhase::Idle:
            if (detectPreparation(headHistory_)) {
                currentPhase_ = HeaderPhase::Preparation;
                phaseStartTime_ = timestamp;
                peakHeadVelocity_ = 0.0f;
            }
            break;

        case HeaderPhase::Preparation: {
            // Track peak velocity during preparation
            float currentSpeed = headHistory_.getCurrentSpeed();
            if (currentSpeed > peakHeadVelocity_) {
                peakHeadVelocity_ = currentSpeed;
            }

            // Check minimum time in phase
            if (timestamp - phaseStartTime_ >= MIN_PREPARATION_TIME) {
                if (detectContact(headHistory_)) {
                    currentPhase_ = HeaderPhase::Contact;
                    phaseStartTime_ = timestamp;
                    headerDirection_ = calculateHeaderDirection();
                }
            }

            // Timeout if preparation takes too long
            if (timestamp - phaseStartTime_ > 2000000) { // 2 seconds
                reset();
            }
            break;
        }

        case HeaderPhase::Contact:
            // Contact is brief, quickly move to recovery
            if (timestamp - phaseStartTime_ >= MIN_CONTACT_TIME) {
                if (detectRecovery(headHistory_)) {
                    currentPhase_ = HeaderPhase::Recovery;
                    phaseStartTime_ = timestamp;
                }
            }
            break;

        case HeaderPhase::Recovery:
            // Complete header after recovery period
            if (timestamp - phaseStartTime_ > 300000) { // 0.3 seconds
                completeHeader();
                reset();
            }
            break;

        default:
            break;
    }
}

bool HeaderDetector::detectPreparation(const MotionHistory& headHistory) {
    if (!headHistory.hasEnoughData()) {
        return false;
    }

    float speed = headHistory.getCurrentSpeed();
    k4a_float3_t velocity = headHistory.getCurrentVelocity();

    // Preparation involves upward and forward motion
    // Y > 0 (upward), Z > 0 (forward in Kinect coordinates)
    return speed > MIN_HEAD_VELOCITY &&
           (velocity.xyz.y > 0.0f || velocity.xyz.z > 0.0f);
}

bool HeaderDetector::detectContact(const MotionHistory& headHistory) {
    if (!headHistory.hasEnoughData()) {
        return false;
    }

    // Contact detected by sudden deceleration
    float currentSpeed = headHistory.getCurrentSpeed();
    float previousSpeed = 0.0f;

    k4a_float3_t prevVel;
    if (headHistory.getVelocity(1, prevVel)) {
        previousSpeed = magnitude(prevVel);
    }

    // Deceleration threshold
    return previousSpeed > MIN_HEAD_VELOCITY &&
           currentSpeed < previousSpeed * DECELERATION_THRESHOLD;
}

bool HeaderDetector::detectRecovery(const MotionHistory& headHistory) {
    if (!headHistory.hasEnoughData()) {
        return false;
    }

    // Recovery is slower motion returning to neutral
    float speed = headHistory.getCurrentSpeed();
    return speed < MIN_HEAD_VELOCITY * 0.5f;
}

k4a_float3_t HeaderDetector::calculateHeaderDirection() const {
    // Direction based on head velocity at peak
    k4a_float3_t direction = headHistory_.getAverageVelocity(3);
    return normalize(direction);
}

HeaderType HeaderDetector::classifyHeaderType(const k4abt_skeleton_t& skeleton,
                                               const MotionHistory& headHistory) {
    k4a_float3_t velocity = headHistory.getCurrentVelocity();
    float speed = magnitude(velocity);

    // Get body position to determine diving vs standing
    k4a_float3_t head = skeleton.joints[K4ABT_JOINT_HEAD].position;
    k4a_float3_t pelvis = skeleton.joints[K4ABT_JOINT_PELVIS].position;
    float bodyLean = angleBetweenVectors(
        subtract(head, pelvis),
        k4a_float3_t{0.0f, 1.0f, 0.0f} // Vertical
    );

    // Classification heuristics
    if (bodyLean > 45.0f) {
        return HeaderType::GlidingHeader; // Diving header
    } else if (speed > POWER_HEADER_VELOCITY && velocity.xyz.y < 0) {
        return HeaderType::PowerHeader; // Downward power header
    } else if (std::abs(velocity.xyz.x) > std::abs(velocity.xyz.z)) {
        return HeaderType::FlickOn; // Glancing header
    } else if (velocity.xyz.y > 0) {
        return HeaderType::DefensiveClear; // Upward clearance
    }

    return HeaderType::PowerHeader; // Default
}

HeaderQuality HeaderDetector::analyzeHeaderQuality(const k4abt_skeleton_t& skeleton,
                                                    const MotionHistory& headHistory,
                                                    HeaderType type) {
    HeaderQuality quality;

    // Head velocity
    quality.headVelocity = peakHeadVelocity_;

    // Neck angle
    quality.neckAngle = calculateNeckAngle(skeleton);

    // Body alignment
    quality.bodyAlignment = calculateBodyAlignment(skeleton);

    // Power score based on velocity
    quality.powerScore = std::min(100.0f, (quality.headVelocity / 4.0f) * 100.0f);

    // Timing score (simplified - based on peak velocity achievement)
    quality.timingScore = quality.headVelocity > MIN_HEAD_VELOCITY * 1.5f ? 80.0f : 60.0f;

    // Overall score
    quality.overallScore = (quality.powerScore * 0.4f +
                           quality.timingScore * 0.3f +
                           quality.bodyAlignment * 0.3f);

    return quality;
}

void HeaderDetector::completeHeader() {
    if (!headerCallback_) {
        return;
    }

    HeaderResult result;
    result.timestamp = currentTimestamp_;
    result.isValid = true;
    result.direction = headerDirection_;

    // Classify header type
    result.type = classifyHeaderType(currentSkeleton_, headHistory_);

    // Analyze quality
    result.quality = analyzeHeaderQuality(currentSkeleton_, headHistory_, result.type);

    headerCallback_(result);
}

void HeaderDetector::reset() {
    currentPhase_ = HeaderPhase::Idle;
    phaseStartTime_ = 0;
    peakHeadVelocity_ = 0.0f;
    headerDirection_ = {0.0f, 0.0f, 0.0f};
}

float HeaderDetector::calculateNeckAngle(const k4abt_skeleton_t& skeleton) const {
    k4a_float3_t head = skeleton.joints[K4ABT_JOINT_HEAD].position;
    k4a_float3_t neck = skeleton.joints[K4ABT_JOINT_NECK].position;
    k4a_float3_t spineChest = skeleton.joints[K4ABT_JOINT_SPINE_CHEST].position;

    k4a_float3_t neckToHead = subtract(head, neck);
    k4a_float3_t spineToNeck = subtract(neck, spineChest);

    return angleBetweenVectors(neckToHead, spineToNeck);
}

float HeaderDetector::calculateBodyAlignment(const k4abt_skeleton_t& skeleton) const {
    // Good body alignment means torso is aligned with header direction
    k4a_float3_t pelvis = skeleton.joints[K4ABT_JOINT_PELVIS].position;
    k4a_float3_t spineChest = skeleton.joints[K4ABT_JOINT_SPINE_CHEST].position;

    k4a_float3_t torsoVector = subtract(spineChest, pelvis);

    // Compare with header direction
    float alignment = dotProduct(normalize(torsoVector), normalize(headerDirection_));

    // Convert to 0-100 score
    return (alignment + 1.0f) * 50.0f; // -1 to 1 becomes 0 to 100
}

float HeaderDetector::magnitude(const k4a_float3_t& v) {
    return std::sqrt(v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z);
}

k4a_float3_t HeaderDetector::normalize(const k4a_float3_t& v) {
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

k4a_float3_t HeaderDetector::subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.x - b.xyz.x,
        a.xyz.y - b.xyz.y,
        a.xyz.z - b.xyz.z
    };
}

float HeaderDetector::dotProduct(const k4a_float3_t& a, const k4a_float3_t& b) {
    return a.xyz.x * b.xyz.x + a.xyz.y * b.xyz.y + a.xyz.z * b.xyz.z;
}

float HeaderDetector::angleBetweenVectors(const k4a_float3_t& a, const k4a_float3_t& b) {
    k4a_float3_t normA = normalize(a);
    k4a_float3_t normB = normalize(b);

    float dot = dotProduct(normA, normB);
    dot = std::max(-1.0f, std::min(1.0f, dot));

    return std::acos(dot) * (180.0f / 3.14159265359f);
}

} // namespace motion
} // namespace kinect

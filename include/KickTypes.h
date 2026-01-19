#ifndef KINECT_FOOTBALL_KICK_TYPES_H
#define KINECT_FOOTBALL_KICK_TYPES_H

#include <k4abt.h>
#include <cstdint>
#include <string>

namespace kinect {

// Kick classification types
enum class KickType {
    Instep,         // Traditional power shot using instep
    SideFootPass,   // Accurate pass using inside of foot
    Outside,        // Outside of foot shot/pass
    Toe,            // Toe poke for quick shots
    Volley,         // Ball struck while in air
    Header,         // Heading the ball
    Unknown
};

// Kick execution phases
enum class KickPhase {
    Idle,           // No kick in progress
    WindUp,         // Leg drawing back
    Acceleration,   // Forward swing toward ball
    Contact,        // Moment of ball contact (estimated)
    FollowThrough   // Completion of kick motion
};

// Foot preference
enum class DominantFoot {
    Left,
    Right,
    Unknown
};

// Comprehensive kick quality metrics
struct KickQuality {
    // Power metrics
    float footVelocity;         // m/s at contact
    float estimatedBallSpeed;   // km/h
    float powerScore;           // 0-100

    // Accuracy metrics
    float directionAngle;       // degrees from target center
    float accuracyScore;        // 0-100

    // Technique metrics
    float kneeAngle;            // degrees at contact
    float hipRotation;          // degrees of rotation
    float followThroughLength;  // meters
    float techniqueScore;       // 0-100

    // Balance metrics
    float bodyLean;             // degrees from vertical
    float balanceScore;         // 0-100

    // Overall
    float overallScore;         // 0-100 (weighted average)

    KickQuality()
        : footVelocity(0.0f)
        , estimatedBallSpeed(0.0f)
        , powerScore(0.0f)
        , directionAngle(0.0f)
        , accuracyScore(0.0f)
        , kneeAngle(0.0f)
        , hipRotation(0.0f)
        , followThroughLength(0.0f)
        , techniqueScore(0.0f)
        , bodyLean(0.0f)
        , balanceScore(0.0f)
        , overallScore(0.0f)
    {}
};

// Complete kick result
struct KickResult {
    KickType type;
    DominantFoot foot;
    KickQuality quality;
    k4a_float3_t kickDirection;
    uint64_t timestamp;         // microseconds
    bool isValid;

    KickResult()
        : type(KickType::Unknown)
        , foot(DominantFoot::Unknown)
        , kickDirection{0.0f, 0.0f, 0.0f}
        , timestamp(0)
        , isValid(false)
    {}
};

// Helper functions
inline const char* kickTypeToString(KickType type) {
    switch (type) {
        case KickType::Instep: return "Instep";
        case KickType::SideFootPass: return "Side Foot Pass";
        case KickType::Outside: return "Outside Foot";
        case KickType::Toe: return "Toe Poke";
        case KickType::Volley: return "Volley";
        case KickType::Header: return "Header";
        default: return "Unknown";
    }
}

inline const char* kickPhaseToString(KickPhase phase) {
    switch (phase) {
        case KickPhase::Idle: return "Idle";
        case KickPhase::WindUp: return "Wind Up";
        case KickPhase::Acceleration: return "Acceleration";
        case KickPhase::Contact: return "Contact";
        case KickPhase::FollowThrough: return "Follow Through";
        default: return "Unknown";
    }
}

inline const char* dominantFootToString(DominantFoot foot) {
    switch (foot) {
        case DominantFoot::Left: return "Left";
        case DominantFoot::Right: return "Right";
        default: return "Unknown";
    }
}

} // namespace kinect

#endif // KINECT_FOOTBALL_KICK_TYPES_H

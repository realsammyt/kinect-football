#ifndef KINECT_FOOTBALL_HEADER_DETECTOR_H
#define KINECT_FOOTBALL_HEADER_DETECTOR_H

#include "MotionHistory.h"
#include "../../include/KickTypes.h"
#include <k4abt.h>
#include <functional>

namespace kinect {
namespace motion {

// Header-specific types
enum class HeaderType {
    PowerHeader,        // Strong downward header
    GlidingHeader,      // Diving header
    FlickOn,           // Glancing header to redirect
    DefensiveClear,    // Clearance header
    Unknown
};

enum class HeaderPhase {
    Idle,
    Setup,             // Tracking ball, positioning
    Preparation,       // Body tensing, neck extending
    Contact,           // Head meets ball
    Recovery           // Return to normal position
};

struct HeaderQuality {
    float headVelocity;        // m/s at contact
    float neckAngle;           // Degrees from neutral
    float bodyAlignment;       // Score 0-100
    float timingScore;         // Score 0-100
    float powerScore;          // Score 0-100
    float overallScore;        // Score 0-100

    HeaderQuality()
        : headVelocity(0.0f)
        , neckAngle(0.0f)
        , bodyAlignment(0.0f)
        , timingScore(0.0f)
        , powerScore(0.0f)
        , overallScore(0.0f)
    {}
};

struct HeaderResult {
    HeaderType type;
    HeaderQuality quality;
    k4a_float3_t direction;
    uint64_t timestamp;
    bool isValid;

    HeaderResult()
        : type(HeaderType::Unknown)
        , direction{0.0f, 0.0f, 0.0f}
        , timestamp(0)
        , isValid(false)
    {}
};

// Callback for header completion
using HeaderCallback = std::function<void(const HeaderResult&)>;

class HeaderDetector {
public:
    // Velocity thresholds for header detection (m/s)
    static constexpr float MIN_HEAD_VELOCITY = 1.0f;
    static constexpr float POWER_HEADER_VELOCITY = 2.5f;

    // Deceleration threshold for contact detection
    static constexpr float DECELERATION_THRESHOLD = 0.6f; // Fraction of velocity

    // Minimum time in phases (microseconds)
    static constexpr uint64_t MIN_PREPARATION_TIME = 150000;  // 0.15s
    static constexpr uint64_t MIN_CONTACT_TIME = 50000;       // 0.05s

    HeaderDetector();
    ~HeaderDetector() = default;

    // Process new skeleton frame
    void processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp);

    // Set callback for header completion
    void setHeaderCallback(HeaderCallback callback) { headerCallback_ = callback; }

    // Get current phase
    HeaderPhase getCurrentPhase() const { return currentPhase_; }

    // Reset detector state
    void reset();

private:
    // Motion history for key joints
    MotionHistory headHistory_;
    MotionHistory neckHistory_;
    MotionHistory spineChestHistory_;
    MotionHistory pelvisHistory_;
    MotionHistory shoulderLeftHistory_;
    MotionHistory shoulderRightHistory_;

    // State tracking
    HeaderPhase currentPhase_;
    uint64_t phaseStartTime_;
    float peakHeadVelocity_;
    k4a_float3_t headerDirection_;

    // Callback
    HeaderCallback headerCallback_;

    // Current skeleton (stored for analysis)
    k4abt_skeleton_t currentSkeleton_;
    uint64_t currentTimestamp_;

    // Phase detection methods
    void updatePhase(uint64_t timestamp);
    bool detectPreparation(const MotionHistory& headHistory);
    bool detectContact(const MotionHistory& headHistory);
    bool detectRecovery(const MotionHistory& headHistory);

    // Calculate header direction
    k4a_float3_t calculateHeaderDirection() const;

    // Classify header type
    HeaderType classifyHeaderType(const k4abt_skeleton_t& skeleton,
                                   const MotionHistory& headHistory);

    // Analyze header quality
    HeaderQuality analyzeHeaderQuality(const k4abt_skeleton_t& skeleton,
                                       const MotionHistory& headHistory,
                                       HeaderType type);

    // Complete header and trigger callback
    void completeHeader();

    // Helper: calculate neck angle
    float calculateNeckAngle(const k4abt_skeleton_t& skeleton) const;

    // Helper: calculate body alignment score
    float calculateBodyAlignment(const k4abt_skeleton_t& skeleton) const;

    // Helper: vector operations
    static float magnitude(const k4a_float3_t& v);
    static k4a_float3_t normalize(const k4a_float3_t& v);
    static k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b);
    static float dotProduct(const k4a_float3_t& a, const k4a_float3_t& b);
    static float angleBetweenVectors(const k4a_float3_t& a, const k4a_float3_t& b);
};

} // namespace motion
} // namespace kinect

#endif // KINECT_FOOTBALL_HEADER_DETECTOR_H

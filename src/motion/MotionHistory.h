#ifndef KINECT_FOOTBALL_MOTION_HISTORY_H
#define KINECT_FOOTBALL_MOTION_HISTORY_H

#include <k4abt.h>
#include <vector>
#include <deque>
#include <cstdint>

namespace kinect {
namespace motion {

// Single frame of joint data
struct JointFrame {
    k4a_float3_t position;
    k4a_float3_t velocity;      // m/s
    uint64_t timestamp;          // microseconds
    float confidence;            // 0.0-1.0

    JointFrame()
        : position{0.0f, 0.0f, 0.0f}
        , velocity{0.0f, 0.0f, 0.0f}
        , timestamp(0)
        , confidence(0.0f)
    {}
};

// Bounded FIFO motion history for a single joint
class MotionHistory {
public:
    static constexpr size_t MAX_HISTORY = 30; // 1 second at 30fps
    static constexpr float MIN_CONFIDENCE = 0.5f;

    MotionHistory();
    ~MotionHistory() = default;

    // Add new joint position
    void addFrame(const k4a_float3_t& position, uint64_t timestamp, float confidence);

    // Get current velocity vector
    k4a_float3_t getCurrentVelocity() const;

    // Get velocity magnitude
    float getCurrentSpeed() const;

    // Get acceleration vector
    k4a_float3_t getCurrentAcceleration() const;

    // Get position N frames ago (0 = current)
    bool getPosition(size_t framesBack, k4a_float3_t& position) const;

    // Get velocity N frames ago (0 = current)
    bool getVelocity(size_t framesBack, k4a_float3_t& velocity) const;

    // Check if we have enough data for analysis
    bool hasEnoughData() const { return frames_.size() >= 3; }

    // Get number of frames stored
    size_t size() const { return frames_.size(); }

    // Clear all history
    void clear();

    // Get average velocity over last N frames
    k4a_float3_t getAverageVelocity(size_t numFrames) const;

    // Get peak velocity in history
    float getPeakSpeed() const;

    // Get time range of history
    float getTimeSpan() const; // seconds

private:
    std::deque<JointFrame> frames_;

    // Calculate velocity between two frames
    k4a_float3_t calculateVelocity(const JointFrame& older, const JointFrame& newer) const;

    // Vector magnitude
    static float magnitude(const k4a_float3_t& v);

    // Vector subtraction
    static k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b);

    // Vector addition
    static k4a_float3_t add(const k4a_float3_t& a, const k4a_float3_t& b);

    // Vector scale
    static k4a_float3_t scale(const k4a_float3_t& v, float s);
};

} // namespace motion
} // namespace kinect

#endif // KINECT_FOOTBALL_MOTION_HISTORY_H

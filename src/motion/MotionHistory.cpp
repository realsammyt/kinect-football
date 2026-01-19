#include "MotionHistory.h"
#include <cmath>
#include <algorithm>

namespace kinect {
namespace motion {

MotionHistory::MotionHistory() {
    frames_.clear();
}

void MotionHistory::addFrame(const k4a_float3_t& position, uint64_t timestamp, float confidence) {
    // Skip low confidence data
    if (confidence < MIN_CONFIDENCE) {
        return;
    }

    JointFrame frame;
    frame.position = position;
    frame.timestamp = timestamp;
    frame.confidence = confidence;

    // Calculate velocity if we have previous frame
    if (!frames_.empty()) {
        frame.velocity = calculateVelocity(frames_.back(), frame);
    }

    // Add to queue
    frames_.push_back(frame);

    // Maintain bounded history using kinect-native pattern
    if (frames_.size() > MAX_HISTORY) {
        frames_.erase(frames_.begin());
    }
}

k4a_float3_t MotionHistory::getCurrentVelocity() const {
    if (frames_.empty()) {
        return {0.0f, 0.0f, 0.0f};
    }
    return frames_.back().velocity;
}

float MotionHistory::getCurrentSpeed() const {
    return magnitude(getCurrentVelocity());
}

k4a_float3_t MotionHistory::getCurrentAcceleration() const {
    if (frames_.size() < 2) {
        return {0.0f, 0.0f, 0.0f};
    }

    const auto& current = frames_.back();
    const auto& previous = frames_[frames_.size() - 2];

    float dt = (current.timestamp - previous.timestamp) / 1000000.0f; // microseconds to seconds
    if (dt <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }

    k4a_float3_t deltaV = subtract(current.velocity, previous.velocity);
    return scale(deltaV, 1.0f / dt);
}

bool MotionHistory::getPosition(size_t framesBack, k4a_float3_t& position) const {
    if (framesBack >= frames_.size()) {
        return false;
    }

    size_t index = frames_.size() - 1 - framesBack;
    position = frames_[index].position;
    return true;
}

bool MotionHistory::getVelocity(size_t framesBack, k4a_float3_t& velocity) const {
    if (framesBack >= frames_.size()) {
        return false;
    }

    size_t index = frames_.size() - 1 - framesBack;
    velocity = frames_[index].velocity;
    return true;
}

void MotionHistory::clear() {
    frames_.clear();
}

k4a_float3_t MotionHistory::getAverageVelocity(size_t numFrames) const {
    if (frames_.empty()) {
        return {0.0f, 0.0f, 0.0f};
    }

    size_t count = std::min(numFrames, frames_.size());
    k4a_float3_t sum = {0.0f, 0.0f, 0.0f};

    for (size_t i = frames_.size() - count; i < frames_.size(); ++i) {
        sum = add(sum, frames_[i].velocity);
    }

    return scale(sum, 1.0f / static_cast<float>(count));
}

float MotionHistory::getPeakSpeed() const {
    if (frames_.empty()) {
        return 0.0f;
    }

    float peak = 0.0f;
    for (const auto& frame : frames_) {
        float speed = magnitude(frame.velocity);
        if (speed > peak) {
            peak = speed;
        }
    }

    return peak;
}

float MotionHistory::getTimeSpan() const {
    if (frames_.size() < 2) {
        return 0.0f;
    }

    uint64_t span = frames_.back().timestamp - frames_.front().timestamp;
    return span / 1000000.0f; // microseconds to seconds
}

k4a_float3_t MotionHistory::calculateVelocity(const JointFrame& older, const JointFrame& newer) const {
    float dt = (newer.timestamp - older.timestamp) / 1000000.0f; // microseconds to seconds

    if (dt <= 0.0f) {
        return {0.0f, 0.0f, 0.0f};
    }

    k4a_float3_t displacement = subtract(newer.position, older.position);
    return scale(displacement, 1.0f / dt);
}

float MotionHistory::magnitude(const k4a_float3_t& v) {
    return std::sqrt(v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z);
}

k4a_float3_t MotionHistory::subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.x - b.xyz.x,
        a.xyz.y - b.xyz.y,
        a.xyz.z - b.xyz.z
    };
}

k4a_float3_t MotionHistory::add(const k4a_float3_t& a, const k4a_float3_t& b) {
    return {
        a.xyz.x + b.xyz.x,
        a.xyz.y + b.xyz.y,
        a.xyz.z + b.xyz.z
    };
}

k4a_float3_t MotionHistory::scale(const k4a_float3_t& v, float s) {
    return {
        v.xyz.x * s,
        v.xyz.y * s,
        v.xyz.z * s
    };
}

} // namespace motion
} // namespace kinect

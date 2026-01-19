#pragma once

#include "KinectDevice.h"
#include <k4abt.h>
#include <vector>
#include <chrono>

namespace kinect {
namespace core {

/**
 * @brief Joint data with position, orientation, and confidence
 */
struct JointData {
    k4a_float3_t position;           // Position in mm
    k4a_quaternion_t orientation;    // Orientation quaternion
    k4abt_joint_confidence_level_t confidence;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Complete body skeleton data
 */
struct BodyData {
    uint32_t id = 0;
    std::vector<JointData> joints;   // K4ABT_JOINT_COUNT joints
    std::chrono::steady_clock::time_point timestamp;

    // Computed velocity (updated by motion analysis)
    k4a_float3_t velocity = {0, 0, 0};
    bool isActive = false;

    BodyData() : joints(K4ABT_JOINT_COUNT) {}
};

/**
 * @brief Azure Kinect Body Tracking wrapper
 *
 * Processes depth frames through the body tracking SDK
 * to produce skeleton data for up to 6 bodies.
 */
class BodyTracker {
public:
    BodyTracker();
    ~BodyTracker();

    // Non-copyable
    BodyTracker(const BodyTracker&) = delete;
    BodyTracker& operator=(const BodyTracker&) = delete;

    /**
     * @brief Initialize body tracker
     * @param device Reference to initialized KinectDevice
     * @return true if successful
     */
    bool initialize(KinectDevice& device);

    /**
     * @brief Shutdown tracker and release resources
     */
    void shutdown();

    /**
     * @brief Process a single capture through body tracking
     * @param capture The capture to process
     * @param timeoutMs Timeout for GPU processing (default 33ms = 1 frame)
     * @return true if processing was successful
     */
    bool processCapture(k4a_capture_t capture, int32_t timeoutMs = 33);

    /**
     * @brief Get the current body frame
     * @param frame Output body frame handle
     * @return true if a frame is available
     */
    bool getBodyFrame(k4abt_frame_t& frame);

    /**
     * @brief Simplified API: process and return body data
     * @return Vector of detected bodies (may be empty)
     */
    std::vector<BodyData> processFrame();

    /**
     * @brief Check if tracker is initialized
     */
    bool isInitialized() const { return tracker_ != nullptr; }

    /**
     * @brief Set GPU device ID for CUDA processing
     */
    void setGpuDeviceId(int deviceId);

private:
    k4abt_tracker_t tracker_ = nullptr;
    k4abt_tracker_configuration_t config_;

    k4a_calibration_t calibration_;
    bool hasFrame_ = false;

    void extractBodyData(k4abt_frame_t frame, std::vector<BodyData>& bodies);

    void logInfo(const std::string& msg);
    void logError(const std::string& msg);
    void logWarning(const std::string& msg);
};

} // namespace core
} // namespace kinect

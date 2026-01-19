#include "BodyTracker.h"
#include <iostream>

namespace kinect {
namespace core {

BodyTracker::BodyTracker() {
    // Default configuration: GPU processing
    config_.sensor_orientation = K4ABT_SENSOR_ORIENTATION_DEFAULT;
    config_.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU;
    config_.gpu_device_id = 0;
}

BodyTracker::~BodyTracker() {
    shutdown();
}

bool BodyTracker::initialize(KinectDevice& device) {
    if (tracker_ != nullptr) {
        logWarning("Tracker already initialized");
        return true;
    }

    if (!device.isInitialized()) {
        logError("Device not initialized");
        return false;
    }

    calibration_ = device.getCalibration();

    k4a_result_t result = k4abt_tracker_create(&calibration_, config_, &tracker_);
    if (result != K4A_RESULT_SUCCEEDED) {
        logError("Failed to create body tracker");
        tracker_ = nullptr;
        return false;
    }

    logInfo("Body tracker initialized (GPU mode)");
    return true;
}

void BodyTracker::shutdown() {
    if (tracker_) {
        k4abt_tracker_shutdown(tracker_);
        k4abt_tracker_destroy(tracker_);
        tracker_ = nullptr;
        logInfo("Body tracker shut down");
    }
}

bool BodyTracker::processCapture(k4a_capture_t capture, int32_t timeoutMs) {
    if (!tracker_ || !capture) {
        return false;
    }

    // Enqueue capture for processing (non-blocking)
    k4a_wait_result_t enqueueResult = k4abt_tracker_enqueue_capture(
        tracker_, capture, K4A_WAIT_RESULT_TIMEOUT);

    if (enqueueResult == K4A_WAIT_RESULT_FAILED) {
        logError("Failed to enqueue capture");
        return false;
    }

    hasFrame_ = true;
    return true;
}

bool BodyTracker::getBodyFrame(k4abt_frame_t& frame) {
    if (!tracker_ || !hasFrame_) {
        return false;
    }

    // Wait for GPU processing to complete
    // Use 33ms timeout (one frame at 30fps) to avoid blocking too long
    k4a_wait_result_t result = k4abt_tracker_pop_result(tracker_, &frame, 33);

    if (result == K4A_WAIT_RESULT_SUCCEEDED) {
        return true;
    } else if (result == K4A_WAIT_RESULT_TIMEOUT) {
        // GPU still processing, not an error
        return false;
    } else {
        logError("Failed to get body frame");
        return false;
    }
}

std::vector<BodyData> BodyTracker::processFrame() {
    std::vector<BodyData> bodies;

    k4abt_frame_t frame = nullptr;
    if (!getBodyFrame(frame)) {
        return bodies;
    }

    extractBodyData(frame, bodies);
    k4abt_frame_release(frame);

    return bodies;
}

void BodyTracker::extractBodyData(k4abt_frame_t frame, std::vector<BodyData>& bodies) {
    uint32_t numBodies = k4abt_frame_get_num_bodies(frame);
    auto timestamp = std::chrono::steady_clock::now();

    bodies.reserve(numBodies);

    for (uint32_t i = 0; i < numBodies; i++) {
        BodyData body;
        body.id = k4abt_frame_get_body_id(frame, i);
        body.timestamp = timestamp;
        body.isActive = true;

        k4abt_skeleton_t skeleton;
        if (k4abt_frame_get_body_skeleton(frame, i, &skeleton) == K4A_RESULT_SUCCEEDED) {
            for (int j = 0; j < K4ABT_JOINT_COUNT; j++) {
                body.joints[j].position = skeleton.joints[j].position;
                body.joints[j].orientation = skeleton.joints[j].orientation;
                body.joints[j].confidence = skeleton.joints[j].confidence_level;
                body.joints[j].timestamp = timestamp;
            }
        }

        bodies.push_back(std::move(body));
    }
}

void BodyTracker::setGpuDeviceId(int deviceId) {
    if (tracker_) {
        logWarning("Cannot change GPU device after initialization");
        return;
    }
    config_.gpu_device_id = deviceId;
}

void BodyTracker::logInfo(const std::string& msg) {
    std::cout << "[BodyTracker] " << msg << std::endl;
}

void BodyTracker::logError(const std::string& msg) {
    std::cerr << "[BodyTracker ERROR] " << msg << std::endl;
}

void BodyTracker::logWarning(const std::string& msg) {
    std::cout << "[BodyTracker WARNING] " << msg << std::endl;
}

} // namespace core
} // namespace kinect

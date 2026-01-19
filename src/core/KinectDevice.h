#pragma once

#include <k4a/k4a.h>
#include <k4abt.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace kinect {
namespace core {

/**
 * @brief Image frame data structure
 */
struct ImageFrame {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int stride = 0;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Azure Kinect device wrapper
 *
 * Handles device lifecycle, configuration, and frame capture.
 * Thread-safe for capture operations.
 */
class KinectDevice {
public:
    KinectDevice();
    ~KinectDevice();

    // Non-copyable
    KinectDevice(const KinectDevice&) = delete;
    KinectDevice& operator=(const KinectDevice&) = delete;

    /**
     * @brief Initialize the Kinect device
     * @param deviceIndex Device index (0 for first device)
     * @return true if successful
     */
    bool initialize(uint32_t deviceIndex = 0);

    /**
     * @brief Configure device settings (call before startCapture)
     */
    void setDepthMode(k4a_depth_mode_t mode);
    void setColorResolution(k4a_color_resolution_t resolution);
    void setFps(k4a_fps_t fps);

    /**
     * @brief Start capturing frames
     * @return true if successful
     */
    bool startCapture();

    /**
     * @brief Stop capturing frames
     */
    void stopCapture();

    /**
     * @brief Capture a single frame
     * @return true if a frame was captured
     */
    bool captureFrame();

    /**
     * @brief Shutdown device and release resources
     */
    void shutdown();

    // Image extraction (thread-safe)
    bool extractColorFrame(ImageFrame& outFrame);
    bool extractDepthFrame(ImageFrame& outFrame);

    // Status
    bool isInitialized() const { return device_ != nullptr; }
    bool isCapturing() const { return capturing_; }

    // Access for tracker initialization
    k4a_device_t getDeviceHandle() const { return device_; }
    k4a_capture_t getCurrentCapture() const { return capture_; }
    k4a_calibration_t getCalibration() const { return calibration_; }

    // Device info
    std::string getSerialNumber() const;
    std::string getFirmwareVersion() const;

private:
    k4a_device_t device_ = nullptr;
    k4a_capture_t capture_ = nullptr;
    k4a_calibration_t calibration_;

    k4a_device_configuration_t config_;
    bool capturing_ = false;

    void logInfo(const std::string& msg);
    void logError(const std::string& msg);
    void logWarning(const std::string& msg);
};

} // namespace core
} // namespace kinect

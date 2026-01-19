#include "KinectDevice.h"
#include <iostream>
#include <cstring>

namespace kinect {
namespace core {

KinectDevice::KinectDevice() {
    // Default configuration for soccer kiosk
    config_ = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config_.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    config_.color_resolution = K4A_COLOR_RESOLUTION_720P;
    config_.camera_fps = K4A_FRAMES_PER_SECOND_30;
    config_.synchronized_images_only = false;  // Don't wait for color sync
    config_.depth_delay_off_color_usec = 0;
    config_.wired_sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
    config_.subordinate_delay_off_master_usec = 0;
    config_.disable_streaming_indicator = false;
}

KinectDevice::~KinectDevice() {
    shutdown();
}

bool KinectDevice::initialize(uint32_t deviceIndex) {
    if (device_ != nullptr) {
        logWarning("Device already initialized");
        return true;
    }

    uint32_t deviceCount = k4a_device_get_installed_count();
    if (deviceCount == 0) {
        logError("No Azure Kinect devices found");
        return false;
    }

    if (deviceIndex >= deviceCount) {
        logError("Device index " + std::to_string(deviceIndex) +
                 " out of range. Found " + std::to_string(deviceCount) + " devices.");
        return false;
    }

    if (k4a_device_open(deviceIndex, &device_) != K4A_RESULT_SUCCEEDED) {
        logError("Failed to open device " + std::to_string(deviceIndex));
        device_ = nullptr;
        return false;
    }

    // Get calibration data
    if (k4a_device_get_calibration(device_, config_.depth_mode,
                                    config_.color_resolution, &calibration_) != K4A_RESULT_SUCCEEDED) {
        logError("Failed to get device calibration");
        k4a_device_close(device_);
        device_ = nullptr;
        return false;
    }

    logInfo("Kinect device initialized successfully");
    logInfo("  Serial: " + getSerialNumber());
    logInfo("  Firmware: " + getFirmwareVersion());

    return true;
}

void KinectDevice::setDepthMode(k4a_depth_mode_t mode) {
    if (capturing_) {
        logWarning("Cannot change depth mode while capturing");
        return;
    }
    config_.depth_mode = mode;
}

void KinectDevice::setColorResolution(k4a_color_resolution_t resolution) {
    if (capturing_) {
        logWarning("Cannot change color resolution while capturing");
        return;
    }
    config_.color_resolution = resolution;
}

void KinectDevice::setFps(k4a_fps_t fps) {
    if (capturing_) {
        logWarning("Cannot change FPS while capturing");
        return;
    }
    config_.camera_fps = fps;
}

bool KinectDevice::startCapture() {
    if (!device_) {
        logError("Device not initialized");
        return false;
    }

    if (capturing_) {
        logWarning("Already capturing");
        return true;
    }

    if (k4a_device_start_cameras(device_, &config_) != K4A_RESULT_SUCCEEDED) {
        logError("Failed to start cameras");
        return false;
    }

    capturing_ = true;
    logInfo("Camera capture started");
    return true;
}

void KinectDevice::stopCapture() {
    if (!device_ || !capturing_) {
        return;
    }

    k4a_device_stop_cameras(device_);
    capturing_ = false;

    if (capture_) {
        k4a_capture_release(capture_);
        capture_ = nullptr;
    }

    logInfo("Camera capture stopped");
}

bool KinectDevice::captureFrame() {
    if (!device_ || !capturing_) {
        return false;
    }

    // Release previous capture
    if (capture_) {
        k4a_capture_release(capture_);
        capture_ = nullptr;
    }

    // Short timeout to avoid blocking
    int32_t timeout_ms = 33;  // ~30fps
    k4a_wait_result_t result = k4a_device_get_capture(device_, &capture_, timeout_ms);

    if (result == K4A_WAIT_RESULT_SUCCEEDED) {
        return true;
    } else if (result == K4A_WAIT_RESULT_TIMEOUT) {
        // Normal timeout, not an error
        return false;
    } else {
        logError("Failed to capture frame");
        return false;
    }
}

void KinectDevice::shutdown() {
    stopCapture();

    if (device_) {
        k4a_device_close(device_);
        device_ = nullptr;
        logInfo("Kinect device shut down");
    }
}

bool KinectDevice::extractColorFrame(ImageFrame& outFrame) {
    if (!capture_) {
        return false;
    }

    k4a_image_t colorImage = k4a_capture_get_color_image(capture_);
    if (!colorImage) {
        return false;
    }

    outFrame.width = k4a_image_get_width_pixels(colorImage);
    outFrame.height = k4a_image_get_height_pixels(colorImage);
    outFrame.stride = k4a_image_get_stride_bytes(colorImage);
    outFrame.timestamp = std::chrono::steady_clock::now();

    size_t bufferSize = k4a_image_get_size(colorImage);
    outFrame.data.resize(bufferSize);
    memcpy(outFrame.data.data(), k4a_image_get_buffer(colorImage), bufferSize);

    k4a_image_release(colorImage);
    return true;
}

bool KinectDevice::extractDepthFrame(ImageFrame& outFrame) {
    if (!capture_) {
        return false;
    }

    k4a_image_t depthImage = k4a_capture_get_depth_image(capture_);
    if (!depthImage) {
        return false;
    }

    outFrame.width = k4a_image_get_width_pixels(depthImage);
    outFrame.height = k4a_image_get_height_pixels(depthImage);
    outFrame.stride = k4a_image_get_stride_bytes(depthImage);
    outFrame.timestamp = std::chrono::steady_clock::now();

    size_t bufferSize = k4a_image_get_size(depthImage);
    outFrame.data.resize(bufferSize);
    memcpy(outFrame.data.data(), k4a_image_get_buffer(depthImage), bufferSize);

    k4a_image_release(depthImage);
    return true;
}

std::string KinectDevice::getSerialNumber() const {
    if (!device_) return "";

    size_t size = 0;
    k4a_device_get_serialnum(device_, nullptr, &size);

    std::string serial(size, '\0');
    k4a_device_get_serialnum(device_, serial.data(), &size);

    // Remove null terminator
    if (!serial.empty() && serial.back() == '\0') {
        serial.pop_back();
    }
    return serial;
}

std::string KinectDevice::getFirmwareVersion() const {
    if (!device_) return "";

    k4a_hardware_version_t version;
    if (k4a_device_get_version(device_, &version) != K4A_RESULT_SUCCEEDED) {
        return "Unknown";
    }

    return std::to_string(version.rgb.major) + "." +
           std::to_string(version.rgb.minor) + "." +
           std::to_string(version.rgb.iteration);
}

void KinectDevice::logInfo(const std::string& msg) {
    std::cout << "[KinectDevice] " << msg << std::endl;
}

void KinectDevice::logError(const std::string& msg) {
    std::cerr << "[KinectDevice ERROR] " << msg << std::endl;
}

void KinectDevice::logWarning(const std::string& msg) {
    std::cout << "[KinectDevice WARNING] " << msg << std::endl;
}

} // namespace core
} // namespace kinect

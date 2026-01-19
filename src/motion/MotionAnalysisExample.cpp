// Example: Integrating Kick Detection and Motion Analysis
// This demonstrates how to use the motion analysis system in your application

#include "KickDetector.h"
#include "KickAnalyzer.h"
#include "HeaderDetector.h"
#include "../core/BodyTracker.h"
#include <iostream>
#include <iomanip>

using namespace kinect;
using namespace kinect::motion;

class MotionAnalysisDemo {
public:
    MotionAnalysisDemo() {
        // Initialize detectors
        kickDetector_ = std::make_unique<KickDetector>();
        kickAnalyzer_ = std::make_unique<KickAnalyzer>();
        headerDetector_ = std::make_unique<HeaderDetector>();

        // Set up callbacks
        kickDetector_->setKickCallback([this](const KickResult& result) {
            onKickDetected(result);
        });

        headerDetector_->setHeaderCallback([this](const HeaderResult& result) {
            onHeaderDetected(result);
        });

        // Configure target zone for accuracy scoring
        TargetZone target;
        target.center = {0.0f, 1.5f, 3.0f}; // 3 meters forward, 1.5m high
        target.radius = 0.5f;                // 0.5m radius
        kickAnalyzer_->setTargetZone(target);
    }

    void processFrame(const k4abt_skeleton_t& skeleton, uint64_t timestamp) {
        // Update both detectors with every frame
        kickDetector_->processSkeleton(skeleton, timestamp);
        headerDetector_->processSkeleton(skeleton, timestamp);

        // Log current detection state
        logDetectionState();
    }

private:
    std::unique_ptr<KickDetector> kickDetector_;
    std::unique_ptr<KickAnalyzer> kickAnalyzer_;
    std::unique_ptr<HeaderDetector> headerDetector_;

    void onKickDetected(const KickResult& result) {
        std::cout << "\n========== KICK DETECTED ==========\n";
        std::cout << "Type: " << kickTypeToString(result.type) << "\n";
        std::cout << "Foot: " << dominantFootToString(result.foot) << "\n";
        std::cout << "Timestamp: " << result.timestamp << " us\n";

        std::cout << "\n--- Quality Metrics ---\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Foot Velocity: " << result.quality.footVelocity << " m/s\n";
        std::cout << "Ball Speed: " << result.quality.estimatedBallSpeed << " km/h\n";
        std::cout << "Power Score: " << result.quality.powerScore << "/100\n";

        std::cout << "\nDirection Angle: " << result.quality.directionAngle << "°\n";
        std::cout << "Accuracy Score: " << result.quality.accuracyScore << "/100\n";

        std::cout << "\nKnee Angle: " << result.quality.kneeAngle << "°\n";
        std::cout << "Hip Rotation: " << result.quality.hipRotation << "°\n";
        std::cout << "Follow Through: " << result.quality.followThroughLength << " m\n";
        std::cout << "Technique Score: " << result.quality.techniqueScore << "/100\n";

        std::cout << "\nBody Lean: " << result.quality.bodyLean << "°\n";
        std::cout << "Balance Score: " << result.quality.balanceScore << "/100\n";

        std::cout << "\n>>> OVERALL SCORE: " << result.quality.overallScore << "/100 <<<\n";
        std::cout << "====================================\n\n";
    }

    void onHeaderDetected(const HeaderResult& result) {
        std::cout << "\n========== HEADER DETECTED ==========\n";
        std::cout << "Type: " << headerTypeToString(result.type) << "\n";
        std::cout << "Timestamp: " << result.timestamp << " us\n";

        std::cout << "\n--- Quality Metrics ---\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Head Velocity: " << result.quality.headVelocity << " m/s\n";
        std::cout << "Neck Angle: " << result.quality.neckAngle << "°\n";
        std::cout << "Body Alignment: " << result.quality.bodyAlignment << "/100\n";
        std::cout << "Timing Score: " << result.quality.timingScore << "/100\n";
        std::cout << "Power Score: " << result.quality.powerScore << "/100\n";

        std::cout << "\n>>> OVERALL SCORE: " << result.quality.overallScore << "/100 <<<\n";
        std::cout << "======================================\n\n";
    }

    void logDetectionState() {
        // Periodically log detection state (every 30 frames = 1 second at 30fps)
        static int frameCount = 0;
        if (++frameCount % 30 == 0) {
            std::cout << "[State] Kick: " << kickPhaseToString(kickDetector_->getCurrentPhase())
                      << " | Header: " << headerPhaseToString(headerDetector_->getCurrentPhase())
                      << "\r" << std::flush;
        }
    }

    const char* headerTypeToString(HeaderType type) {
        switch (type) {
            case HeaderType::PowerHeader: return "Power Header";
            case HeaderType::GlidingHeader: return "Gliding Header";
            case HeaderType::FlickOn: return "Flick On";
            case HeaderType::DefensiveClear: return "Defensive Clear";
            default: return "Unknown";
        }
    }

    const char* headerPhaseToString(HeaderPhase phase) {
        switch (phase) {
            case HeaderPhase::Idle: return "Idle";
            case HeaderPhase::Setup: return "Setup";
            case HeaderPhase::Preparation: return "Preparation";
            case HeaderPhase::Contact: return "Contact";
            case HeaderPhase::Recovery: return "Recovery";
            default: return "Unknown";
        }
    }
};

// Example integration with main application loop
void exampleMainLoop() {
    MotionAnalysisDemo demo;

    // Assume you have a BodyTracker from the core module
    // kinect::core::BodyTracker bodyTracker;

    // Main loop
    while (true) {
        // Get skeleton from your body tracker
        // k4abt_frame_t bodyFrame = bodyTracker.getNextFrame();
        // k4abt_skeleton_t skeleton;
        // k4abt_frame_get_body_skeleton(bodyFrame, 0, &skeleton);

        // Get current timestamp (microseconds)
        // uint64_t timestamp = ...; // From frame or system clock

        // Process with motion analysis
        // demo.processFrame(skeleton, timestamp);

        // Your other application logic...
    }
}

// Example: Batch analysis of recorded session
void exampleBatchAnalysis() {
    std::cout << "=== Batch Analysis Example ===\n";
    std::cout << "1. Load recorded skeleton data from file\n";
    std::cout << "2. Process each frame through motion detectors\n";
    std::cout << "3. Aggregate statistics:\n";
    std::cout << "   - Total kicks detected\n";
    std::cout << "   - Average kick speed\n";
    std::cout << "   - Accuracy distribution\n";
    std::cout << "   - Technique scores\n";
    std::cout << "4. Export results to CSV/JSON\n";
}

// Example: Real-time feedback system
void exampleRealTimeFeedback() {
    std::cout << "=== Real-Time Feedback Example ===\n";
    std::cout << "1. Detect kick phases in real-time\n";
    std::cout << "2. Display visual feedback:\n";
    std::cout << "   - Wind-up phase: 'GET READY!'\n";
    std::cout << "   - Acceleration: 'KICK NOW!'\n";
    std::cout << "   - Contact: Show impact visualization\n";
    std::cout << "   - Result: Display score and metrics\n";
    std::cout << "3. Play audio feedback based on quality\n";
    std::cout << "4. Update leaderboard\n";
}

int main() {
    std::cout << "Kinect Football - Motion Analysis System\n";
    std::cout << "=========================================\n\n";

    exampleBatchAnalysis();
    std::cout << "\n";
    exampleRealTimeFeedback();

    return 0;
}

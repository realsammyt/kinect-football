/**
 * Kinect Football - Game Challenge System Example
 *
 * Demonstrates basic usage of the game challenge system.
 * Run any of the three challenges with Azure Kinect.
 */

#include "../src/game/GameManager.h"
#include "../src/game/AccuracyChallenge.h"
#include "../src/game/PowerChallenge.h"
#include "../src/game/PenaltyShootout.h"
#include <k4a/k4a.h>
#include <k4abt.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

using namespace kinect::game;

class KinectFootballGame {
public:
    KinectFootballGame()
        : device_(nullptr)
        , tracker_(nullptr)
        , running_(false)
    {
    }

    ~KinectFootballGame() {
        shutdown();
    }

    bool initialize() {
        // Initialize Kinect device
        if (K4A_RESULT_SUCCEEDED != k4a_device_open(0, &device_)) {
            std::cerr << "Failed to open Kinect device\n";
            return false;
        }

        // Configure camera
        k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
        config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
        config.color_resolution = K4A_COLOR_RESOLUTION_1080P;
        config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        config.camera_fps = K4A_FRAMES_PER_SECOND_30;

        if (K4A_RESULT_SUCCEEDED != k4a_device_start_cameras(device_, &config)) {
            std::cerr << "Failed to start cameras\n";
            return false;
        }

        // Get calibration
        k4a_calibration_t calibration;
        if (K4A_RESULT_SUCCEEDED != k4a_device_get_calibration(
                device_, config.depth_mode, config.color_resolution, &calibration)) {
            std::cerr << "Failed to get calibration\n";
            return false;
        }

        // Create body tracker
        k4abt_tracker_configuration_t trackerConfig = K4ABT_TRACKER_CONFIG_DEFAULT;
        if (K4A_RESULT_SUCCEEDED != k4abt_tracker_create(&calibration, trackerConfig, &tracker_)) {
            std::cerr << "Failed to create body tracker\n";
            return false;
        }

        // Initialize game manager
        GameConfig gameConfig;
        gameManager_ = std::make_unique<GameManager>(gameConfig);
        gameManager_->initialize();

        // Setup callbacks
        setupCallbacks();

        std::cout << "Kinect Football initialized successfully!\n";
        return true;
    }

    void shutdown() {
        if (gameManager_) {
            gameManager_->endSession();
            gameManager_->shutdown();
        }

        if (tracker_) {
            k4abt_tracker_shutdown(tracker_);
            k4abt_tracker_destroy(tracker_);
            tracker_ = nullptr;
        }

        if (device_) {
            k4a_device_stop_cameras(device_);
            k4a_device_close(device_);
            device_ = nullptr;
        }
    }

    void run(ChallengeType challengeType) {
        running_ = true;

        // Start session
        gameManager_->startSession();

        // Start challenge
        if (!gameManager_->startChallenge(challengeType)) {
            std::cerr << "Failed to start challenge\n";
            return;
        }

        std::cout << "\nChallenge started! Press 'q' to quit, 'r' to restart\n";

        auto lastTime = std::chrono::steady_clock::now();

        while (running_) {
            // Calculate delta time
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Get frame
            k4a_capture_t capture = nullptr;
            k4a_wait_result_t result = k4a_device_get_capture(device_, &capture, K4A_WAIT_INFINITE);

            if (result == K4A_WAIT_RESULT_SUCCEEDED) {
                // Queue for body tracking
                if (K4A_WAIT_RESULT_SUCCEEDED == k4abt_tracker_enqueue_capture(tracker_, capture, K4A_WAIT_INFINITE)) {
                    // Get body tracking result
                    k4abt_frame_t bodyFrame = nullptr;
                    if (K4A_WAIT_RESULT_SUCCEEDED == k4abt_tracker_pop_result(tracker_, &bodyFrame, K4A_WAIT_INFINITE)) {
                        // Get color image
                        k4a_image_t colorImage = k4a_capture_get_color_image(capture);

                        // Process frame
                        processFrame(bodyFrame, colorImage, deltaTime);

                        // Render
                        render(colorImage, bodyFrame);

                        // Cleanup
                        if (colorImage) k4a_image_release(colorImage);
                        k4abt_frame_release(bodyFrame);
                    }
                }

                k4a_capture_release(capture);
            }

            // Handle keyboard
            int key = cv::waitKey(1);
            if (key == 'q' || key == 'Q') {
                running_ = false;
            } else if (key == 'r' || key == 'R') {
                // Restart challenge
                gameManager_->stopCurrentChallenge();
                gameManager_->startChallenge(challengeType);
            }

            // Check if challenge completed
            if (!gameManager_->hasActiveChallenge()) {
                std::cout << "\nChallenge complete! Press 'r' to restart, 'q' to quit\n";
            }
        }

        // End session
        gameManager_->endSession();
        displaySessionStats();
    }

private:
    void setupCallbacks() {
        gameManager_->setOnChallengeStart([](ChallengeType type) {
            std::cout << "\n=== Challenge Started ===\n";
            switch (type) {
                case ChallengeType::ACCURACY:
                    std::cout << "Accuracy Challenge - Hit all target zones!\n";
                    break;
                case ChallengeType::POWER:
                    std::cout << "Power Challenge - Kick as hard as you can!\n";
                    break;
                case ChallengeType::PENALTY_SHOOTOUT:
                    std::cout << "Penalty Shootout - Score against the goalkeeper!\n";
                    break;
                default:
                    break;
            }
        });

        gameManager_->setOnChallengeComplete([](const ChallengeResult& result) {
            std::cout << "\n=== Challenge Complete ===\n";
            std::cout << "Final Score: " << result.finalScore << "\n";
            std::cout << "Grade: " << result.grade << "\n";
            std::cout << "Accuracy: " << static_cast<int>(result.accuracy * 100) << "%\n";
            std::cout << "Attempts: " << result.attempts << "\n";
            std::cout << "Successes: " << result.successes << "\n";
            if (result.maxVelocity > 0) {
                std::cout << "Max Velocity: " << static_cast<int>(result.maxVelocity) << " km/h\n";
            }
            std::cout << "Duration: " << static_cast<int>(result.duration) << " seconds\n";
            std::cout << (result.passed ? "PASSED" : "FAILED") << "\n";
        });

        gameManager_->setOnAchievementUnlocked([](const AchievementConfig& achievement) {
            std::cout << "\n*** ACHIEVEMENT UNLOCKED ***\n";
            std::cout << achievement.name << "\n";
            std::cout << achievement.description << "\n";
        });
    }

    void processFrame(k4abt_frame_t bodyFrame, k4a_image_t colorImage, float deltaTime) {
        // Get number of bodies
        uint32_t numBodies = k4abt_frame_get_num_bodies(bodyFrame);

        if (numBodies > 0) {
            // Get first body skeleton
            k4abt_skeleton_t skeleton;
            k4abt_frame_get_body_skeleton(bodyFrame, 0, &skeleton);

            // Get depth image
            k4a_image_t depthImage = k4abt_frame_get_depth_image(bodyFrame);

            // Process in game manager
            gameManager_->processFrame(skeleton, depthImage, deltaTime);

            // Release depth image
            if (depthImage) k4a_image_release(depthImage);
        }
    }

    void render(k4a_image_t colorImage, k4abt_frame_t bodyFrame) {
        // Convert color image to OpenCV Mat
        uint8_t* buffer = k4a_image_get_buffer(colorImage);
        int width = k4a_image_get_width_pixels(colorImage);
        int height = k4a_image_get_height_pixels(colorImage);

        cv::Mat frame(height, width, CV_8UC4, buffer);
        cv::Mat displayFrame;
        cv::cvtColor(frame, displayFrame, cv::COLOR_BGRA2BGR);

        // Render game UI
        gameManager_->render(displayFrame);

        // Draw body skeleton (simple)
        drawSkeleton(displayFrame, bodyFrame);

        // Show frame
        cv::imshow("Kinect Football", displayFrame);
    }

    void drawSkeleton(cv::Mat& frame, k4abt_frame_t bodyFrame) {
        uint32_t numBodies = k4abt_frame_get_num_bodies(bodyFrame);

        for (uint32_t i = 0; i < numBodies; i++) {
            k4abt_skeleton_t skeleton;
            k4abt_frame_get_body_skeleton(bodyFrame, i, &skeleton);

            // Draw simplified skeleton (just a few key joints)
            auto drawJoint = [&](int jointId) {
                auto joint = skeleton.joints[jointId];
                if (joint.confidence_level >= K4ABT_JOINT_CONFIDENCE_MEDIUM) {
                    // Project 3D to 2D (simplified - would need proper calibration)
                    int x = static_cast<int>(joint.position.v[0] * 200 + frame.cols / 2);
                    int y = static_cast<int>(-joint.position.v[1] * 200 + frame.rows / 2);

                    if (x >= 0 && x < frame.cols && y >= 0 && y < frame.rows) {
                        cv::circle(frame, cv::Point(x, y), 5, cv::Scalar(0, 255, 0), -1);
                    }
                }
            };

            // Draw key joints
            drawJoint(K4ABT_JOINT_HEAD);
            drawJoint(K4ABT_JOINT_SPINE_CHEST);
            drawJoint(K4ABT_JOINT_HAND_LEFT);
            drawJoint(K4ABT_JOINT_HAND_RIGHT);
            drawJoint(K4ABT_JOINT_FOOT_LEFT);
            drawJoint(K4ABT_JOINT_FOOT_RIGHT);
        }
    }

    void displaySessionStats() {
        auto stats = gameManager_->getSessionStats();

        std::cout << "\n=== Session Summary ===\n";
        std::cout << "Total Score: " << stats.totalScore << "\n";
        std::cout << "Challenges Completed: " << stats.challengesCompleted << "\n";
        std::cout << "Total Kicks: " << stats.totalKicks << "\n";
        std::cout << "Average Accuracy: " << static_cast<int>(stats.avgAccuracy * 100) << "%\n";
        std::cout << "Max Velocity: " << static_cast<int>(stats.maxVelocity) << " km/h\n";
        std::cout << "Session Duration: " << static_cast<int>(stats.sessionDuration) << " seconds\n";

        if (!stats.achievementsUnlocked.empty()) {
            std::cout << "\nAchievements Unlocked:\n";
            for (const auto& id : stats.achievementsUnlocked) {
                std::cout << "  - " << id << "\n";
            }
        }
    }

    k4a_device_t device_;
    k4abt_tracker_t tracker_;
    std::unique_ptr<GameManager> gameManager_;
    bool running_;
};

int main(int argc, char* argv[]) {
    std::cout << "Kinect Football - Game Challenge System\n";
    std::cout << "========================================\n\n";

    // Select challenge
    ChallengeType challengeType = ChallengeType::ACCURACY;

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "accuracy") {
            challengeType = ChallengeType::ACCURACY;
        } else if (arg == "power") {
            challengeType = ChallengeType::POWER;
        } else if (arg == "penalty") {
            challengeType = ChallengeType::PENALTY_SHOOTOUT;
        } else {
            std::cout << "Usage: " << argv[0] << " [accuracy|power|penalty]\n";
            return 1;
        }
    } else {
        std::cout << "Select challenge:\n";
        std::cout << "1. Accuracy (target zones)\n";
        std::cout << "2. Power (max velocity)\n";
        std::cout << "3. Penalty Shootout\n";
        std::cout << "Enter choice (1-3): ";

        int choice;
        std::cin >> choice;

        switch (choice) {
            case 1: challengeType = ChallengeType::ACCURACY; break;
            case 2: challengeType = ChallengeType::POWER; break;
            case 3: challengeType = ChallengeType::PENALTY_SHOOTOUT; break;
            default:
                std::cout << "Invalid choice\n";
                return 1;
        }
    }

    // Initialize and run game
    KinectFootballGame game;

    if (!game.initialize()) {
        std::cerr << "Failed to initialize game\n";
        return 1;
    }

    game.run(challengeType);

    std::cout << "\nThanks for playing!\n";
    return 0;
}

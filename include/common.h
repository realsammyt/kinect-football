#pragma once

#include <k4a/k4a.hpp>
#include <k4abt.hpp>
#include <array>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

// Logging macros
#define LOG_INFO(msg) std::cout << "[INFO] " << msg << std::endl
#define LOG_WARN(msg) std::cout << "[WARN] " << msg << std::endl
#define LOG_ERROR(msg) std::cerr << "[ERROR] " << msg << std::endl
#define LOG_DEBUG(msg) std::cout << "[DEBUG] " << msg << std::endl

// Constants
constexpr int MAX_BODIES = 6;
constexpr int NUM_JOINTS = static_cast<int>(K4ABT_JOINT_COUNT);
constexpr float KICK_DETECTION_THRESHOLD = 2.0f; // m/s
constexpr float SESSION_TIMEOUT_SECONDS = 60.0f;
constexpr float ATTRACT_MODE_IDLE_TIME = 30.0f;

// Game State Machine
enum class GameState {
    ATTRACT,              // Idle mode, waiting for player
    PLAYER_DETECTED,      // Player detected, showing welcome
    SELECTING_CHALLENGE,  // Player selecting challenge type
    COUNTDOWN,            // 3-2-1 countdown
    PLAYING,              // Active gameplay
    PROCESSING,           // Analyzing results
    RESULTS,              // Showing score/stats
    CELEBRATION,          // Victory animation
    SHARE,                // QR code/email sharing
    THANK_YOU,            // Session complete
    ERROR_STATE           // Error recovery
};

// Challenge Types
enum class ChallengeType {
    NONE,
    ACCURACY,           // Hit targets on goal
    POWER,              // Kick speed measurement
    PENALTY_SHOOTOUT,   // Score penalties vs goalkeeper
    FREE_KICK,          // Curve ball around wall
    SKILL_TEST          // Combined skills challenge
};

// Kick Types
enum class KickType {
    NONE,
    INSTEP,       // Power shot
    INSIDE_FOOT,  // Placement shot
    OUTSIDE_FOOT, // Curve shot
    VOLLEY,       // Mid-air strike
    CHIP          // Lofted shot
};

// Joint data with velocity tracking
struct JointData {
    k4a_float3_t position;
    k4a_float3_t velocity;
    k4a_quaternion_t orientation;
    k4abt_joint_confidence_level_t confidence;
    uint64_t timestamp_us;

    JointData()
        : position{0, 0, 0}
        , velocity{0, 0, 0}
        , orientation{0, 0, 0, 1}
        , confidence(K4ABT_JOINT_CONFIDENCE_NONE)
        , timestamp_us(0)
    {}
};

// Player skeleton data
struct PlayerData {
    uint32_t id;
    bool isActive;
    std::array<JointData, NUM_JOINTS> joints;
    k4a_float3_t centerOfMass;
    float footVelocity;
    uint64_t lastUpdateTime;

    // Tracking state
    bool isKicking;
    bool wasKicking;
    int consecutiveFrames;

    PlayerData()
        : id(0)
        , isActive(false)
        , centerOfMass{0, 0, 0}
        , footVelocity(0.0f)
        , lastUpdateTime(0)
        , isKicking(false)
        , wasKicking(false)
        , consecutiveFrames(0)
    {}

    // Get specific joint
    const JointData& getJoint(k4abt_joint_id_t joint) const {
        return joints[static_cast<size_t>(joint)];
    }

    JointData& getJoint(k4abt_joint_id_t joint) {
        return joints[static_cast<size_t>(joint)];
    }
};

// Kick detection result
struct KickData {
    KickType type;
    float power;           // 0-100
    float direction;       // Angle in degrees
    float accuracy;        // 0-100
    k4a_float3_t footPosition;
    k4a_float3_t footVelocity;
    uint64_t timestamp;
    uint32_t playerId;

    // Ball trajectory prediction
    k4a_float3_t predictedImpactPoint;
    float estimatedBallSpeed;

    KickData()
        : type(KickType::NONE)
        , power(0.0f)
        , direction(0.0f)
        , accuracy(0.0f)
        , footPosition{0, 0, 0}
        , footVelocity{0, 0, 0}
        , timestamp(0)
        , playerId(0)
        , predictedImpactPoint{0, 0, 0}
        , estimatedBallSpeed(0.0f)
    {}
};

// Challenge result
struct ChallengeResult {
    ChallengeType challenge;
    int score;
    int maxScore;
    float accuracy;
    float avgPower;
    int successfulKicks;
    int totalKicks;
    std::vector<KickData> kicks;
    uint64_t durationMs;

    ChallengeResult()
        : challenge(ChallengeType::NONE)
        , score(0)
        , maxScore(100)
        , accuracy(0.0f)
        , avgPower(0.0f)
        , successfulKicks(0)
        , totalKicks(0)
        , durationMs(0)
    {}

    float getPercentage() const {
        return maxScore > 0 ? (100.0f * score / maxScore) : 0.0f;
    }
};

// Session data
struct SessionData {
    std::string sessionId;
    uint32_t playerId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;

    ChallengeType selectedChallenge;
    ChallengeResult result;

    // Sharing
    bool wasShared;
    std::string shareMethod; // "qr", "email", "sms"
    std::string downloadUrl;

    SessionData()
        : sessionId("")
        , playerId(0)
        , selectedChallenge(ChallengeType::NONE)
        , wasShared(false)
        , shareMethod("")
        , downloadUrl("")
    {}

    uint64_t getDurationMs() const {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime
        );
        return static_cast<uint64_t>(duration.count());
    }
};

// Frame data container
struct FrameData {
    k4a::image colorImage;
    k4a::image depthImage;
    k4a::image bodyIndexMap;
    k4abt_frame_t bodyFrame;
    std::vector<PlayerData> players;
    uint64_t timestamp;

    FrameData()
        : bodyFrame(nullptr)
        , timestamp(0)
    {
        players.reserve(MAX_BODIES);
    }

    ~FrameData() {
        if (bodyFrame) {
            k4abt_frame_release(bodyFrame);
            bodyFrame = nullptr;
        }
    }

    // Prevent copying (images are reference-counted)
    FrameData(const FrameData&) = delete;
    FrameData& operator=(const FrameData&) = delete;

    // Allow moving
    FrameData(FrameData&& other) noexcept
        : colorImage(std::move(other.colorImage))
        , depthImage(std::move(other.depthImage))
        , bodyIndexMap(std::move(other.bodyIndexMap))
        , bodyFrame(other.bodyFrame)
        , players(std::move(other.players))
        , timestamp(other.timestamp)
    {
        other.bodyFrame = nullptr;
    }

    FrameData& operator=(FrameData&& other) noexcept {
        if (this != &other) {
            if (bodyFrame) {
                k4abt_frame_release(bodyFrame);
            }
            colorImage = std::move(other.colorImage);
            depthImage = std::move(other.depthImage);
            bodyIndexMap = std::move(other.bodyIndexMap);
            bodyFrame = other.bodyFrame;
            players = std::move(other.players);
            timestamp = other.timestamp;
            other.bodyFrame = nullptr;
        }
        return *this;
    }
};

// Health monitoring
struct HealthMetrics {
    std::atomic<uint64_t> framesProcessed{0};
    std::atomic<uint64_t> framesDropped{0};
    std::atomic<uint64_t> kicksDetected{0};
    std::atomic<uint64_t> sessionsCompleted{0};
    std::atomic<float> avgFps{0.0f};
    std::atomic<bool> kinectHealthy{false};
    std::atomic<bool> trackerHealthy{false};

    std::chrono::system_clock::time_point lastFrameTime;
    std::chrono::system_clock::time_point startTime;

    HealthMetrics() {
        startTime = std::chrono::system_clock::now();
        lastFrameTime = startTime;
    }

    void reset() {
        framesProcessed = 0;
        framesDropped = 0;
        kicksDetected = 0;
        // Don't reset sessionsCompleted
        avgFps = 0.0f;
        startTime = std::chrono::system_clock::now();
        lastFrameTime = startTime;
    }
};

// Utility functions
namespace util {
    inline float magnitude(const k4a_float3_t& v) {
        return std::sqrt(v.xyz.x * v.xyz.x + v.xyz.y * v.xyz.y + v.xyz.z * v.xyz.z);
    }

    inline k4a_float3_t subtract(const k4a_float3_t& a, const k4a_float3_t& b) {
        return k4a_float3_t{
            a.xyz.x - b.xyz.x,
            a.xyz.y - b.xyz.y,
            a.xyz.z - b.xyz.z
        };
    }

    inline k4a_float3_t normalize(const k4a_float3_t& v) {
        float mag = magnitude(v);
        if (mag < 1e-6f) return k4a_float3_t{0, 0, 0};
        return k4a_float3_t{
            v.xyz.x / mag,
            v.xyz.y / mag,
            v.xyz.z / mag
        };
    }

    inline float dot(const k4a_float3_t& a, const k4a_float3_t& b) {
        return a.xyz.x * b.xyz.x + a.xyz.y * b.xyz.y + a.xyz.z * b.xyz.z;
    }

    inline std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time_t);

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &tm);
        return std::string(buffer);
    }

    inline std::string generateSessionId() {
        return "session_" + getCurrentTimestamp();
    }
}

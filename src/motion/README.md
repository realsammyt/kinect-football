# Kinect Football - Motion Analysis System

Biomechanical kick detection and analysis for FIFA 2026 soccer simulator using Azure Kinect body tracking.

## Overview

This system provides real-time detection and quality analysis of soccer kicks and headers using Azure Kinect DK skeletal tracking data. It implements a multi-phase state machine for motion detection and comprehensive biomechanical analysis for performance scoring.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Motion Analysis System                    │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────┐      ┌──────────────┐                      │
│  │   Skeleton  │──────▶│ MotionHistory│                      │
│  │   Frames    │      │  (30 frames)  │                      │
│  └─────────────┘      └──────┬───────┘                      │
│                               │                               │
│                      ┌────────▼────────┐                     │
│                      │  KickDetector   │                     │
│                      │  (Phase FSM)    │                     │
│                      └────────┬────────┘                     │
│                               │                               │
│                      ┌────────▼────────┐                     │
│                      │  KickAnalyzer   │                     │
│                      │  (Quality Calc) │                     │
│                      └────────┬────────┘                     │
│                               │                               │
│                      ┌────────▼────────┐                     │
│                      │   KickResult    │                     │
│                      │   (Callback)    │                     │
│                      └─────────────────┘                     │
│                                                               │
│  ┌─────────────┐      ┌──────────────┐                      │
│  │   Skeleton  │──────▶│HeaderDetector│                      │
│  │   Frames    │      │  (Phase FSM)  │                      │
│  └─────────────┘      └──────┬───────┘                      │
│                               │                               │
│                      ┌────────▼────────┐                     │
│                      │  HeaderResult   │                     │
│                      │   (Callback)    │                     │
│                      └─────────────────┘                     │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

## Components

### 1. KickTypes.h
Defines data structures for kick classification and quality metrics.

**Key Types:**
- `KickType`: Instep, SideFootPass, Outside, Toe, Volley, Header
- `KickPhase`: Idle, WindUp, Acceleration, Contact, FollowThrough
- `KickQuality`: Power, accuracy, technique, and balance metrics
- `KickResult`: Complete kick analysis with all metrics

### 2. MotionHistory
Bounded FIFO buffer storing joint position history for motion analysis.

**Features:**
- Stores last 30 frames (1 second at 30fps)
- Calculates velocity and acceleration
- Filters low-confidence data (< 0.5)
- Kinect-native pattern: `erase(begin())` for FIFO

**Key Methods:**
```cpp
void addFrame(const k4a_float3_t& position, uint64_t timestamp, float confidence);
k4a_float3_t getCurrentVelocity() const;
float getCurrentSpeed() const;
k4a_float3_t getCurrentAcceleration() const;
float getPeakSpeed() const;
```

### 3. KickDetector
State machine for detecting kick phases and triggering analysis.

**Phase Transitions:**
```
Idle ──▶ WindUp ──▶ Acceleration ──▶ Contact ──▶ FollowThrough ──▶ Complete
  ▲                                                                      │
  └──────────────────────────────────────────────────────────────────────┘
```

**Velocity Thresholds:**
- `VELOCITY_WINDUP = 0.5 m/s` - Backward leg motion
- `VELOCITY_ACCELERATION = 2.0 m/s` - Forward swing
- `VELOCITY_IDLE = 0.3 m/s` - Return to rest

**Key Methods:**
```cpp
void processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp);
void setKickCallback(KickCallback callback);
KickPhase getCurrentPhase() const;
DominantFoot getDominantFoot() const;
```

### 4. KickAnalyzer
Comprehensive biomechanical analysis of completed kicks.

**Scoring Components:**
- **Power (30%)**: Foot velocity → estimated ball speed (km/h)
- **Accuracy (25%)**: Direction angle vs target zone
- **Technique (25%)**: Knee angle, hip rotation, follow-through
- **Balance (20%)**: Body lean, stability

**Analysis Methods:**
```cpp
KickResult analyzeKick(
    const k4abt_skeleton_t& skeleton,
    const MotionHistory& ankleHistory,
    const MotionHistory& footHistory,
    const MotionHistory& kneeHistory,
    const MotionHistory& hipHistory,
    const MotionHistory& pelvisHistory,
    DominantFoot foot,
    uint64_t timestamp
);

KickType classifyKickType(...);
void setTargetZone(const TargetZone& target);
```

**Reference Values:**
- Max ball speed: 120 km/h (professional level)
- Ideal knee angle: 135°
- Max hip rotation: 90°

### 5. HeaderDetector
Specialized detector for heading the ball.

**Phase Transitions:**
```
Idle ──▶ Preparation ──▶ Contact ──▶ Recovery ──▶ Complete
  ▲                                                    │
  └────────────────────────────────────────────────────┘
```

**Header Types:**
- `PowerHeader`: Strong downward header
- `GlidingHeader`: Diving header
- `FlickOn`: Glancing redirect
- `DefensiveClear`: Upward clearance

**Key Methods:**
```cpp
void processSkeleton(const k4abt_skeleton_t& skeleton, uint64_t timestamp);
void setHeaderCallback(HeaderCallback callback);
HeaderPhase getCurrentPhase() const;
```

## Usage Example

### Basic Integration

```cpp
#include "KickDetector.h"
#include "KickAnalyzer.h"
#include "HeaderDetector.h"

using namespace kinect::motion;

// Initialize
KickDetector kickDetector;
KickAnalyzer kickAnalyzer;
HeaderDetector headerDetector;

// Set up callbacks
kickDetector.setKickCallback([](const KickResult& result) {
    std::cout << "Kick detected: " << kickTypeToString(result.type) << "\n";
    std::cout << "Ball speed: " << result.quality.estimatedBallSpeed << " km/h\n";
    std::cout << "Overall score: " << result.quality.overallScore << "/100\n";
});

headerDetector.setHeaderCallback([](const HeaderResult& result) {
    std::cout << "Header detected!\n";
    std::cout << "Power: " << result.quality.powerScore << "/100\n";
});

// Configure target zone
TargetZone target;
target.center = {0.0f, 1.5f, 3.0f}; // 3m forward, 1.5m high
target.radius = 0.5f;                // 0.5m radius
kickAnalyzer.setTargetZone(target);

// Main loop
while (true) {
    k4abt_skeleton_t skeleton = getNextSkeleton();
    uint64_t timestamp = getCurrentTimestamp();

    kickDetector.processSkeleton(skeleton, timestamp);
    headerDetector.processSkeleton(skeleton, timestamp);
}
```

### Advanced: Custom Analysis

```cpp
// Access detailed metrics
void onKickDetected(const KickResult& result) {
    const auto& q = result.quality;

    // Power analysis
    float footSpeed = q.footVelocity;        // m/s
    float ballSpeed = q.estimatedBallSpeed;  // km/h
    float powerScore = q.powerScore;         // 0-100

    // Accuracy analysis
    float angle = q.directionAngle;          // degrees from target
    float accuracyScore = q.accuracyScore;   // 0-100

    // Technique analysis
    float kneeAngle = q.kneeAngle;           // degrees
    float hipRotation = q.hipRotation;       // degrees
    float followThrough = q.followThroughLength; // meters
    float techniqueScore = q.techniqueScore; // 0-100

    // Balance analysis
    float bodyLean = q.bodyLean;             // degrees from vertical
    float balanceScore = q.balanceScore;     // 0-100

    // Overall
    float overallScore = q.overallScore;     // weighted average

    // Kick metadata
    KickType type = result.type;
    DominantFoot foot = result.foot;
    k4a_float3_t direction = result.kickDirection;
}
```

## Azure Kinect Joint Indices

The system uses these Azure Kinect Body Tracking joint indices:

```cpp
K4ABT_JOINT_PELVIS = 0
K4ABT_JOINT_SPINE_NAVAL = 1
K4ABT_JOINT_SPINE_CHEST = 2
K4ABT_JOINT_NECK = 3
K4ABT_JOINT_HEAD = 26

K4ABT_JOINT_HIP_LEFT = 18
K4ABT_JOINT_KNEE_LEFT = 19
K4ABT_JOINT_ANKLE_LEFT = 20
K4ABT_JOINT_FOOT_LEFT = 21

K4ABT_JOINT_HIP_RIGHT = 22
K4ABT_JOINT_KNEE_RIGHT = 23
K4ABT_JOINT_ANKLE_RIGHT = 24
K4ABT_JOINT_FOOT_RIGHT = 25

K4ABT_JOINT_SHOULDER_LEFT = 5
K4ABT_JOINT_SHOULDER_RIGHT = 12
```

## Performance Considerations

### Frame Rate
- Designed for 30 FPS operation
- Motion history: 1 second = 30 frames
- Phase timeouts prevent stuck states

### Confidence Filtering
- Minimum confidence threshold: 0.5
- Low-confidence frames are skipped
- Robust to temporary occlusion

### Memory Usage
- Each MotionHistory: ~30 frames × 32 bytes = ~1 KB
- KickDetector: ~9 histories = ~9 KB
- HeaderDetector: ~6 histories = ~6 KB
- Total per detector: ~15 KB (minimal overhead)

### CPU Efficiency
- No expensive operations in hot path
- Vector math using simple operations
- State machine: O(1) phase transitions
- History maintenance: O(1) with bounded queue

## Calibration and Tuning

### Velocity Thresholds
Adjust in detector headers based on your setup:

```cpp
// KickDetector.h
static constexpr float VELOCITY_WINDUP = 0.5f;       // Lower = more sensitive
static constexpr float VELOCITY_ACCELERATION = 2.0f; // Higher = faster kicks only
static constexpr float VELOCITY_IDLE = 0.3f;         // Lower = stricter reset

// HeaderDetector.h
static constexpr float MIN_HEAD_VELOCITY = 1.0f;     // Minimum header speed
static constexpr float POWER_HEADER_VELOCITY = 2.5f; // Power header threshold
```

### Timing Parameters
Adjust phase durations:

```cpp
// Minimum time in each phase (microseconds)
static constexpr uint64_t MIN_WINDUP_TIME = 200000;      // 0.2s
static constexpr uint64_t MIN_ACCELERATION_TIME = 100000; // 0.1s
static constexpr uint64_t MIN_PREPARATION_TIME = 150000;  // 0.15s
```

### Target Zone Configuration
Position and size for accuracy scoring:

```cpp
TargetZone target;
target.center = {0.0f, 1.5f, 3.0f}; // X, Y, Z in meters
target.radius = 0.5f;                // Radius in meters
kickAnalyzer.setTargetZone(target);
```

## Testing

### Unit Testing
Test individual components:
```cpp
// Test MotionHistory
MotionHistory history;
for (int i = 0; i < 35; ++i) {
    history.addFrame(position, timestamp, confidence);
}
assert(history.size() == 30); // Bounded to MAX_HISTORY

// Test phase transitions
KickDetector detector;
// Feed test data simulating a kick
// Verify phase transitions occur correctly
```

### Integration Testing
Test complete pipeline:
1. Load recorded skeleton data
2. Process through detectors
3. Verify expected kicks are detected
4. Check quality metrics are reasonable

### Performance Testing
- Process 1000 frames
- Measure average processing time per frame
- Target: < 1ms per frame on modern CPU

## Future Enhancements

### Potential Improvements
- [ ] Machine learning for kick type classification
- [ ] Ball trajectory prediction from kick vector
- [ ] Multi-person tracking (opponent blocking detection)
- [ ] Fatigue detection from motion quality degradation
- [ ] Injury risk assessment from technique analysis
- [ ] Real-time coaching feedback
- [ ] Comparative analysis vs professional players

### Sensor Fusion
- Combine with ball tracking camera
- IMU data for acceleration validation
- Force plates for ground reaction forces

## License

Part of the Kinect Football project. See main project LICENSE.

## References

- [Azure Kinect Body Tracking SDK](https://docs.microsoft.com/en-us/azure/kinect-dk/body-sdk-download)
- [Azure Kinect Joint Hierarchy](https://docs.microsoft.com/en-us/azure/kinect-dk/body-joints)
- [Biomechanics of Soccer Kicking](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5450428/)
- [Motion Analysis in Sports](https://www.springer.com/gp/book/9783642372261)

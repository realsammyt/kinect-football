# Kinect Football - Quick Reference Card

## File Locations

```
kinect-football/
├── include/
│   └── GameConfig.h              ← All configurations
├── src/game/
│   ├── ChallengeBase.h/cpp       ← Abstract base
│   ├── AccuracyChallenge.h/cpp   ← Target zones
│   ├── PowerChallenge.h/cpp      ← Max velocity
│   ├── PenaltyShootout.h/cpp     ← AI goalkeeper
│   ├── ScoringEngine.h/cpp       ← Points & achievements
│   ├── GameManager.h/cpp         ← Orchestration
│   └── CMakeLists.txt            ← Build config
├── examples/
│   └── game_example.cpp          ← Working example
└── docs/
    ├── BUILD_GUIDE.md            ← Setup instructions
    ├── GAME_SYSTEM_SUMMARY.md    ← Complete overview
    └── QUICK_REFERENCE.md        ← This file
```

## Build & Run (Windows)

```bash
# Setup
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release

# Run
cd Release
game_example.exe accuracy   # Target zones
game_example.exe power       # Max velocity
game_example.exe penalty     # AI goalkeeper
```

## Basic Usage

```cpp
#include "GameManager.h"

GameManager manager;
manager.initialize();
manager.startSession();
manager.startChallenge(ChallengeType::ACCURACY);

while (running) {
    manager.processFrame(skeleton, depth, deltaTime);
    manager.render(frame);
}

manager.endSession();
```

## Challenge Types

| Challenge | Code | Description |
|-----------|------|-------------|
| Accuracy | `ChallengeType::ACCURACY` | Hit 9 target zones |
| Power | `ChallengeType::POWER` | Max velocity (3 tries) |
| Penalty | `ChallengeType::PENALTY_SHOOTOUT` | Beat goalkeeper (5 kicks) |

## State Machine

```
IDLE → INSTRUCTIONS → COUNTDOWN → ACTIVE → COMPLETE
```

## Achievements (9 total)

### Accuracy
- **Bullseye** - Hit all 9 zones
- **Corner Specialist** - Hit 4 corners
- **Sharpshooter** - 80%+ accuracy (10+ kicks)

### Power
- **Thunderstrike** - 100+ km/h
- **Rocket Shot** - 120+ km/h
- **Consistent Power** - 3× 80+ km/h

### Penalty
- **Perfect Five** - 5/5 goals
- **Ice Cold** - Win sudden death
- **Penalty Master** - 20+ lifetime goals

## Configuration Quick Edit

In `GameConfig.h`:

```cpp
// Easy mode
config.accuracyConfig.timeLimitSeconds = 90.0f;
config.penaltyConfig.goalkeeperCoverage = 0.4f;

// Hard mode
config.accuracyConfig.timeLimitSeconds = 45.0f;
config.penaltyConfig.goalkeeperCoverage = 0.9f;

// Scoring
config.scoring.basePoints = 100;
config.scoring.accuracyMultiplier = 1.5f;
```

## Event Callbacks

```cpp
manager.setOnChallengeStart([](ChallengeType type) {
    // Challenge started
});

manager.setOnChallengeComplete([](const ChallengeResult& result) {
    // Challenge finished
    // result.finalScore, result.grade, result.accuracy
});

manager.setOnAchievementUnlocked([](const AchievementConfig& achievement) {
    // Achievement unlocked
    // achievement.name, achievement.description
});
```

## Common Issues

| Problem | Solution |
|---------|----------|
| "k4a.dll not found" | Add SDK bin to PATH |
| Body tracking slow | Use Release build + GPU |
| Kick not detecting | Adjust thresholds in source |
| OpenCV missing | Copy DLLs to output folder |

## Performance Targets

- **FPS**: 30 constant
- **Latency**: < 100ms
- **Memory**: < 500 MB
- **CPU**: < 50%

## Key Classes

| Class | Purpose |
|-------|---------|
| `GameManager` | Orchestration |
| `ChallengeBase` | Base class |
| `AccuracyChallenge` | Target shooting |
| `PowerChallenge` | Max velocity |
| `PenaltyShootout` | Goalkeeper AI |
| `ScoringEngine` | Points & grades |

## Grading Scale

| Grade | Range | Color |
|-------|-------|-------|
| S | 95%+ | Gold |
| A | 85-94% | Green |
| B | 70-84% | Yellow |
| C | 55-69% | Orange |
| D | 40-54% | Red |
| F | < 40% | Gray |

## Zone Multipliers (Accuracy)

```
3x | 2x | 3x    (Top)
2x | 1x | 2x    (Middle)
3x | 2x | 3x    (Bottom)
```

## Velocity Ratings (Power)

| km/h | Rating |
|------|--------|
| < 40 | Too Slow |
| 40-70 | Weak |
| 70-100 | Good |
| 100-120 | Excellent |
| 120+ | World Class |

## Session Stats

```cpp
struct SessionStats {
    int32_t totalScore;
    int32_t challengesCompleted;
    int32_t totalKicks;
    float avgAccuracy;
    float maxVelocity;
    float sessionDuration;
    vector<string> achievementsUnlocked;
};
```

## Challenge Results

```cpp
struct ChallengeResult {
    int32_t finalScore;
    int32_t attempts;
    int32_t successes;
    float accuracy;      // 0-1
    float maxVelocity;   // km/h
    float duration;      // seconds
    bool passed;
    string grade;        // S, A, B, C, D, F
};
```

## Kick Detection States

```cpp
enum class KickState {
    IDLE,           // Waiting
    WINDING_UP,     // Foot back
    KICKING,        // Forward motion
    FOLLOW_THROUGH  // After impact
};
```

## Dependencies

- Azure Kinect SDK 1.4.1+
- Azure Kinect Body Tracking SDK 1.1.0+
- OpenCV 4.5.0+
- C++17 compiler
- CMake 3.15+

## Support

- Docs: `src/game/README.md`
- Build: `BUILD_GUIDE.md`
- Summary: `GAME_SYSTEM_SUMMARY.md`
- Example: `examples/game_example.cpp`

---

**Version**: 1.0.0 | **Date**: 2026-01-18 | **Status**: Production Ready

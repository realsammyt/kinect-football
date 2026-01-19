# Kinect Football - Game Challenge System

Complete gamified soccer challenge system for FIFA 2026 simulator kiosk.

## Architecture Overview

```
GameManager
    ├── ChallengeBase (abstract)
    │   ├── AccuracyChallenge
    │   ├── PowerChallenge
    │   └── PenaltyShootout
    ├── ScoringEngine
    └── Leaderboard
```

## Core Components

### 1. GameConfig.h
Central configuration for all challenges, scoring, and achievements.

**Key Structures:**
- `ScoringConfig` - Base points, multipliers, streak bonuses
- `AccuracyChallengeConfig` - 3x3 target grid, time limits
- `PowerChallengeConfig` - Velocity thresholds, attempts
- `PenaltyShootoutConfig` - Goalkeeper AI parameters
- `AchievementConfig` - Unlock conditions

### 2. ChallengeBase (Abstract Class)
Base class implementing state machine pattern.

**States:**
- `IDLE` - Not started
- `INSTRUCTIONS` - Showing how to play
- `COUNTDOWN` - 3-2-1 countdown
- `ACTIVE` - Challenge in progress
- `PAUSED` - Temporarily paused
- `COMPLETE` - Challenge finished

**Virtual Methods:**
```cpp
virtual void start();
virtual void processFrame(skeleton, depth, deltaTime);
virtual void finish();
virtual void render(cv::Mat& frame) = 0;
virtual std::string getName() const = 0;
```

### 3. AccuracyChallenge
Hit target zones in a 3x3 grid.

**Features:**
- Dynamic target selection (prioritizes unhit zones)
- Zone multipliers: Corners 3x, Edges 2x, Center 1x
- Combo streak bonuses
- Completion bonus for hitting all 9 zones

**Kick Detection:**
1. Track foot position history
2. Detect wind-up (foot moving back)
3. Detect impact (fast forward motion)
4. Estimate trajectory from foot direction
5. Map impact point to 3x3 grid

### 4. PowerChallenge
Maximum velocity kicks (3 attempts).

**Velocity Ratings:**
- Weak: 40-70 km/h
- Good: 70-100 km/h
- Excellent: 100-120 km/h
- World Class: 120+ km/h

**Technique Scoring:**
- Hip rotation detection
- Knee bend analysis
- Form multipliers (up to 2x)

**Visual Features:**
- Real-time power meter
- Animated velocity display
- Attempt history

### 5. PenaltyShootout
Classic 5-penalty shootout with AI goalkeeper.

**Goalkeeper AI:**
- Predicts kick direction from body pose
- Configurable reaction time, coverage, randomness
- Adjacent zone partial saves
- Dive animation

**Game Modes:**
- Best of 5 kicks
- Sudden death if tied (optional)

**Kick States:**
1. `POSITIONING` - Getting ready
2. `AIMING` - Showing aim guide
3. `WINDUP` - Winding up
4. `KICKED` - Ball in flight
5. `RESULT_SHOW` - Display goal/save
6. `NEXT_ROUND` - Advance

### 6. ScoringEngine
Calculates scores with multipliers and bonuses.

**Score Components:**
```cpp
struct ScoreBreakdown {
    int32_t baseScore;
    int32_t accuracyBonus;
    int32_t powerBonus;
    int32_t techniqueBonus;
    int32_t comboBonus;
    int32_t timeBonus;
    int32_t completionBonus;
};
```

**Combo System:**
- Tracks consecutive successes
- Time window (3 seconds)
- Increasing multipliers

### 7. GameManager
Orchestrates challenge lifecycle and session management.

**Responsibilities:**
- Challenge creation/destruction
- Frame processing delegation
- Session statistics tracking
- Achievement checking
- Event callbacks

## Achievements

### Accuracy Achievements
- **Bullseye** - Hit all 9 zones
- **Corner Specialist** - Hit all 4 corners
- **Sharpshooter** - 80%+ accuracy with 10+ kicks

### Power Achievements
- **Thunderstrike** - 100+ km/h kick
- **Rocket Shot** - 120+ km/h kick
- **Consistent Power** - Three 80+ km/h kicks

### Penalty Achievements
- **Perfect Five** - Score all 5 penalties
- **Ice Cold** - Win in sudden death
- **Penalty Master** - 20+ lifetime penalty goals

## Usage Example

```cpp
#include "GameManager.h"

// Initialize
GameConfig config;
GameManager manager(config);
manager.initialize();

// Set callbacks
manager.setOnChallengeStart([](ChallengeType type) {
    std::cout << "Challenge started!\n";
});

manager.setOnChallengeComplete([](const ChallengeResult& result) {
    std::cout << "Score: " << result.finalScore << "\n";
    std::cout << "Grade: " << result.grade << "\n";
});

manager.setOnAchievementUnlocked([](const AchievementConfig& achievement) {
    std::cout << "Achievement Unlocked: " << achievement.name << "\n";
});

// Start session
manager.startSession();

// Start accuracy challenge
manager.startChallenge(ChallengeType::ACCURACY);

// Game loop
while (running) {
    k4abt_skeleton_t skeleton = /* get from body tracker */;
    k4a_image_t depth = /* get depth image */;
    float deltaTime = /* calculate */;

    // Process frame
    manager.processFrame(skeleton, depth, deltaTime);

    // Render
    cv::Mat frame = /* get camera frame */;
    manager.render(frame);
    cv::imshow("Kinect Football", frame);

    // Handle completion
    if (!manager.hasActiveChallenge()) {
        // Challenge complete, show results or start new
    }
}

// End session
manager.endSession();
auto stats = manager.getSessionStats();
```

## Kick Detection Algorithm

All challenges use similar kick detection:

```cpp
enum class KickState {
    IDLE,           // Waiting for wind-up
    WINDING_UP,     // Foot moving back
    KICKING,        // Forward motion detected
    FOLLOW_THROUGH  // Post-impact
};

// Detection steps:
1. Track foot joint position history (last 10 frames)
2. Calculate foot velocity vector
3. Detect wind-up: deltaZ > threshold (moving away)
4. Detect impact: deltaZ < -threshold (fast forward)
5. Estimate trajectory from foot direction
6. Apply cooldown before next kick
```

## Customization

### Adding New Challenges

1. Create class inheriting from `ChallengeBase`
2. Implement virtual methods
3. Add configuration struct to `GameConfig.h`
4. Update `GameManager::createChallenge()`

```cpp
class MyChallenge : public ChallengeBase {
public:
    MyChallenge(const MyConfig& config);

    void processFrame(...) override;
    void render(cv::Mat& frame) override;

    std::string getName() const override {
        return "My Challenge";
    }
};
```

### Tuning Difficulty

Edit configurations in `GameConfig.h`:

```cpp
// Make accuracy easier
config.accuracyConfig.minimumAccuracyForPass = 0.3f;  // Lower threshold
config.accuracyConfig.timeLimitSeconds = 90.0f;      // More time

// Make goalkeeper easier
config.penaltyConfig.goalkeeperCoverage = 0.5f;     // Reduced coverage
config.penaltyConfig.goalkeeperReactionTime = 0.5f; // Slower reaction

// Adjust scoring
config.accuracyConfig.scoring.basePoints = 200;     // Higher base
```

### Custom Achievements

Add to `AchievementRegistry::getDefaultAchievements()`:

```cpp
{
    "my_achievement",
    "Achievement Name",
    "Description",
    "assets/achievements/icon.png",
    ChallengeType::ACCURACY,
    1000,  // required score
    10,    // required attempts
    0.9f,  // required accuracy
    0.0f   // required velocity
}
```

## File Structure

```
include/
    GameConfig.h                 - All configurations and enums

src/game/
    ChallengeBase.h/cpp         - Abstract base class
    AccuracyChallenge.h/cpp     - Target zone challenge
    PowerChallenge.h/cpp        - Maximum velocity challenge
    PenaltyShootout.h/cpp       - Penalty shootout
    ScoringEngine.h/cpp         - Points and achievements
    GameManager.h/cpp           - Challenge orchestration
    README.md                    - This file
```

## Dependencies

- Azure Kinect SDK (`k4a`, `k4abt`)
- OpenCV (`cv::Mat` for rendering)
- C++17 (std::optional, structured bindings)

## Performance Notes

- Kick detection runs at frame rate (30 fps)
- Minimal memory allocation during gameplay
- Position history limited to last 10 frames
- State machines prevent redundant calculations

## Visual Feedback

All challenges provide:
- Real-time stats overlay
- Target/goal visualization
- Countdown animations
- Result screens with grades (S, A, B, C, D, F)
- Achievement unlock notifications

## Future Enhancements

Potential additions:
- **Free Kick Challenge** - Curve ball around wall
- **Skill Move Challenge** - Gesture combos
- **Multiplayer Mode** - Head-to-head competitions
- **Career Mode** - Progressive difficulty
- **Replay System** - Record and playback kicks
- **Online Leaderboards** - Global rankings

## Testing

Recommended test scenarios:
1. Complete each challenge from start to finish
2. Test achievement unlocks
3. Verify scoring calculations
4. Test state transitions (pause/resume)
5. Check edge cases (no kicks, timeout)
6. Validate goalkeeper AI variety

## Integration with Kiosk

The game system integrates with the larger kiosk application:

```cpp
// In main kiosk loop
if (kioskState == KioskState::PLAYING_CHALLENGE) {
    gameManager.processFrame(skeleton, depth, deltaTime);
    gameManager.render(displayFrame);

    if (!gameManager.hasActiveChallenge()) {
        // Return to menu
        kioskState = KioskState::MENU;
    }
}
```

---

**Author:** Claude (AI-assisted development)
**Date:** 2026-01-18
**Version:** 1.0.0

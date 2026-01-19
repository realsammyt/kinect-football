# Kinect Football - FIFA 2026 Soccer Simulator Kiosk

## Project Overview

**kinect-football** is an interactive soccer simulator kiosk designed for FIFA 2026 fan zones, sports venues, and entertainment centers. It uses Azure Kinect body tracking to detect player movements, analyze kick mechanics, and create an immersive gamified soccer experience.

## Goals

1. **Real-time motion tracking** - Detect kicks, headers, dribbling motions at 30fps
2. **Gamified challenges** - Accuracy tests, power meters, skill challenges with scoring
3. **Multi-player support** - Turn-based and head-to-head competitive modes
4. **Social integration** - Leaderboards, photo capture, social media sharing
5. **Kiosk-ready** - Auto-recovery, attract mode, robust error handling

## Architecture Overview

### Reused from kinect-native

| Component | Source | Adaptation |
|-----------|--------|------------|
| 3-thread architecture | Application.cpp | Same pattern: capture → analysis → GUI |
| Ring buffer | RingBuffer<T,N> | Template reused directly |
| BodyData structures | common.h | Extended with velocity vectors |
| Memory bounding | All assessments | Same max-frame limits |
| Thread safety patterns | onKinectRestart | Join-before-destroy pattern |
| GUI framework | ImGui + DirectX11 | Reskinned for soccer theme |
| Report generation | ReportGenerator | JSON export for analytics |

### New Soccer-Specific Modules

```
kinect-football/
├── include/
│   ├── common.h                    # Shared types, player data, game state
│   ├── KickTypes.h                 # Kick classification enums
│   └── GameConfig.h                # Challenge configurations
│
├── src/
│   ├── core/
│   │   ├── KinectDevice.cpp/h      # Azure Kinect abstraction (from kinect-native)
│   │   ├── BodyTracker.cpp/h       # 32-joint skeleton tracking
│   │   ├── PlayerTracker.cpp/h     # Multi-player state management
│   │   └── RingBuffer.h            # Thread-safe ring buffer
│   │
│   ├── motion/
│   │   ├── KickDetector.cpp/h      # Kick detection pipeline
│   │   ├── KickAnalyzer.cpp/h      # Power, accuracy, technique scoring
│   │   ├── HeaderDetector.cpp/h    # Header motion detection
│   │   ├── DribbleDetector.cpp/h   # Ball control motion patterns
│   │   └── MotionHistory.cpp/h     # Bounded velocity/position history
│   │
│   ├── game/
│   │   ├── GameManager.cpp/h       # Challenge state machine
│   │   ├── ChallengeBase.cpp/h     # Base class for all challenges
│   │   ├── AccuracyChallenge.cpp/h # Target zone shooting
│   │   ├── PowerChallenge.cpp/h    # Maximum kick power test
│   │   ├── SkillChallenge.cpp/h    # Combined technique scoring
│   │   ├── PenaltyShootout.cpp/h   # Classic penalty kicks
│   │   ├── FreeKickChallenge.cpp/h # Free kick accuracy
│   │   └── ScoringEngine.cpp/h     # Points, multipliers, achievements
│   │
│   ├── social/
│   │   ├── LeaderboardManager.cpp/h # Local + cloud leaderboards
│   │   ├── PlayerProfile.cpp/h      # Player sessions, history
│   │   ├── PhotoCapture.cpp/h       # Screenshot + overlay generation
│   │   ├── ShareManager.cpp/h       # Social media integration
│   │   └── AchievementSystem.cpp/h  # Badges, unlocks, progression
│   │
│   ├── gui/
│   │   ├── Application.cpp/h        # 3-thread main loop
│   │   ├── KioskRenderer.cpp/h      # Soccer-themed ImGui panels
│   │   ├── AttractMode.cpp/h        # Idle screen, demos
│   │   ├── GameplayHUD.cpp/h        # In-game overlay
│   │   ├── ResultsScreen.cpp/h      # Post-challenge results
│   │   ├── LeaderboardView.cpp/h    # Top scores display
│   │   └── VFXSystem.cpp/h          # Particle effects, celebrations
│   │
│   ├── audio/
│   │   ├── AudioManager.cpp/h       # Sound effect playback
│   │   ├── CrowdAmbience.cpp/h      # Stadium atmosphere
│   │   └── CommentarySystem.cpp/h   # Audio feedback clips
│   │
│   └── kiosk/
│       ├── KioskManager.cpp/h       # Auto-recovery, health monitoring
│       ├── SessionManager.cpp/h     # Player session lifecycle
│       └── AnalyticsLogger.cpp/h    # Usage telemetry
│
├── assets/
│   ├── fonts/                       # FIFA-style fonts
│   ├── textures/                    # UI elements, targets, badges
│   ├── sounds/                      # Kick sounds, crowd, commentary
│   └── shaders/                     # VFX shaders
│
├── CMakeLists.txt
└── README.md
```

## Phase 1: Core Framework (Week 1)

### 1.1 Project Setup
- [ ] Initialize Git repository with .gitignore
- [ ] Create CMakeLists.txt with Azure Kinect SDK integration
- [ ] Port KinectDevice and BodyTracker from kinect-native
- [ ] Port RingBuffer and common data structures
- [ ] Port 3-thread Application architecture

### 1.2 Player Tracking Foundation
- [ ] Create PlayerTracker for multi-body management
- [ ] Implement player identification (closest to sensor, left/right zones)
- [ ] Add bounded MotionHistory with velocity calculation
- [ ] Implement joint velocity tracking (ankle, knee, hip)

### 1.3 Basic GUI Shell
- [ ] Set up ImGui + DirectX11 rendering
- [ ] Create AttractMode idle screen
- [ ] Implement basic state machine (Attract → Playing → Results)
- [ ] Add F12 device restart from kinect-native

## Phase 2: Kick Detection (Week 2)

### 2.1 Kick Detection Pipeline
- [ ] Implement foot trajectory tracking (ANKLE_LEFT/RIGHT, FOOT_LEFT/RIGHT)
- [ ] Detect kick phases: wind-up → acceleration → contact → follow-through
- [ ] Calculate kick velocity at peak (m/s → km/h display)
- [ ] Detect kick direction vector (2D projection onto virtual goal)

### 2.2 Kick Classification
```cpp
enum class KickType {
    Instep,      // Standard power shot
    SideFootPass,// Inside foot pass
    Outside,     // Curved shot
    Toe,         // Toe poke
    Volley,      // Airborne kick
    Header       // Head contact
};
```
- [ ] Train simple classifiers for kick type
- [ ] Extract technique features (knee angle, hip rotation, follow-through)

### 2.3 Accuracy Detection
- [ ] Define virtual goal zones (3x3 grid = 9 zones)
- [ ] Map kick direction vector to target zone
- [ ] Score accuracy based on zone difficulty
- [ ] Detect "saved" vs "scored" for goalkeeper mode

## Phase 3: Gamification (Week 3)

### 3.1 Challenge Framework
```cpp
class ChallengeBase {
public:
    virtual void start() = 0;
    virtual void processFrame(const BodyData& body) = 0;
    virtual void finish() = 0;
    virtual ChallengeResult getResult() = 0;

protected:
    ChallengeState state_;  // Idle, Instructions, Countdown, Active, Complete
    float timeRemaining_;
    int score_;
};
```

### 3.2 Challenge Types
1. **Accuracy Challenge** - Hit designated zones, increasing difficulty
2. **Power Challenge** - Maximum kick speed measurement
3. **Penalty Shootout** - 5 kicks, beat the virtual keeper
4. **Free Kick Challenge** - Curve around wall, hit corners
5. **Skill Challenge** - Combined scoring: power + accuracy + technique

### 3.3 Scoring System
```cpp
struct ScoringConfig {
    int basePointsPerGoal = 100;
    float accuracyMultiplier = 1.5f;    // Corner zones
    float powerMultiplier = 1.2f;       // >100 km/h
    float techniqueMultiplier = 1.3f;   // Good form detected
    float comboMultiplier = 0.1f;       // Per consecutive goal
};
```

### 3.4 Achievement Badges
- "Sniper" - 5 corner goals in one session
- "Powerhouse" - Kick over 120 km/h
- "Perfect 10" - 10 consecutive goals
- "Hat Trick" - 3 goals in penalty shootout
- "Golden Boot" - Top of daily leaderboard

## Phase 4: Social Features (Week 4)

### 4.1 Leaderboard System
- [ ] SQLite local database for scores
- [ ] Cloud sync via REST API (optional Azure backend)
- [ ] Time-scoped views: Today, This Week, All Time
- [ ] Challenge-specific leaderboards

### 4.2 Player Profiles
- [ ] QR code check-in for returning players
- [ ] Session history and personal bests
- [ ] Badge collection display
- [ ] Statistics dashboard

### 4.3 Photo/Video Capture
- [ ] Screenshot on goal with celebration overlay
- [ ] Player silhouette + score composite
- [ ] QR code for download link
- [ ] Optional: short video clips of best kicks

### 4.4 Social Sharing
- [ ] Generate shareable images with branding
- [ ] Deep links to leaderboard position
- [ ] Optional Twitter/Instagram API integration

## Phase 5: Polish & Kiosk Mode (Week 5)

### 5.1 Visual Effects
- [ ] Particle system for goals (confetti, fireworks)
- [ ] Ball trajectory visualization
- [ ] Power meter animation
- [ ] Target zone highlighting
- [ ] Countdown timer effects

### 5.2 Audio System
- [ ] Kick impact sounds (varied by power)
- [ ] Crowd reactions (cheers, gasps)
- [ ] Goal celebration audio
- [ ] Background stadium ambience
- [ ] UI feedback sounds

### 5.3 Kiosk Hardening
- [ ] Auto-restart on crash (Windows service wrapper)
- [ ] Kinect health monitoring and auto-recovery
- [ ] Session timeout (return to attract mode)
- [ ] Error state handling with user-friendly messages
- [ ] Remote configuration updates

### 5.4 Analytics
- [ ] Session logging (duration, challenges attempted)
- [ ] Performance metrics (kick counts, accuracy rates)
- [ ] Error logging for maintenance
- [ ] Usage heatmaps by time of day

## Technical Specifications

### Hardware Requirements
| Component | Specification |
|-----------|---------------|
| Azure Kinect DK | Required, v1.4.2+ SDK |
| Display | 55"+ 4K, portrait or landscape |
| GPU | GTX 1060+ or equivalent (CUDA for body tracking) |
| CPU | i5-8400 or better |
| RAM | 16GB minimum |
| Storage | 256GB SSD |
| Network | Ethernet for cloud sync |

### Performance Targets
| Metric | Target |
|--------|--------|
| Frame rate | 30 FPS consistent |
| Kick detection latency | <100ms |
| UI response | <16ms |
| Memory usage | <2GB |
| Session startup | <5 seconds |

### Key Dependencies
- Azure Kinect SDK 1.4.2
- Azure Kinect Body Tracking SDK
- Dear ImGui 1.90+
- DirectX 11
- SQLite3
- nlohmann/json
- Optional: OpenCV for ball tracking

## Risk Mitigation

### From kinect-native lessons learned:

1. **Race conditions** - Use join-before-destroy pattern for all threads
2. **Memory leaks** - Bound all history vectors to max frames
3. **Frame drops** - Use 33ms timeout for GPU processing
4. **Busy-wait loops** - Add 1ms sleep on failure paths
5. **Input validation** - Validate all external data (network, config files)
6. **GUI state bugs** - Don't overwrite pending UI state during render

### Soccer-specific risks:

1. **Kick detection accuracy** - Use velocity thresholds + phase detection
2. **Multi-player confusion** - Zone-based player assignment
3. **Occlusion** - Require frontal stance for kick detection
4. **Lighting** - Test in varied kiosk lighting conditions

## Success Metrics

1. **Engagement**: Average session > 3 minutes
2. **Retention**: >30% return players (QR check-in)
3. **Sharing**: >10% sessions generate social share
4. **Reliability**: <1% crash rate, >99% uptime
5. **Performance**: Consistent 30 FPS under load

## Implementation Order

1. **Core** → Device, tracker, ring buffer, 3-thread loop
2. **Motion** → Kick detection, velocity tracking
3. **Game** → Challenge framework, accuracy/power tests
4. **GUI** → Game HUD, results screen, attract mode
5. **Social** → Leaderboards, profiles, photo capture
6. **Polish** → VFX, audio, kiosk hardening
7. **Test** → Integration testing, stress testing
8. **Deploy** → Installer, auto-updater, monitoring

---

## Appendix: Kick Detection Algorithm

```cpp
struct KickPhase {
    enum Type { Idle, WindUp, Acceleration, Contact, FollowThrough };
};

class KickDetector {
public:
    void processFrame(const BodyData& body) {
        // 1. Calculate foot velocities
        float leftFootVel = calculateVelocity(leftAnkleHistory_);
        float rightFootVel = calculateVelocity(rightAnkleHistory_);

        // 2. Detect phase transitions
        float dominantVel = std::max(leftFootVel, rightFootVel);
        bool isLeftDominant = leftFootVel > rightFootVel;

        switch (currentPhase_) {
            case Idle:
                if (dominantVel > WINDUP_THRESHOLD) {
                    currentPhase_ = WindUp;
                    kickStartTime_ = now();
                }
                break;

            case WindUp:
                if (dominantVel > ACCELERATION_THRESHOLD) {
                    currentPhase_ = Acceleration;
                    peakVelocity_ = dominantVel;
                }
                break;

            case Acceleration:
                peakVelocity_ = std::max(peakVelocity_, dominantVel);
                if (dominantVel < peakVelocity_ * 0.7f) {
                    // Velocity decreasing = contact occurred
                    currentPhase_ = Contact;
                    contactTime_ = now();
                    kickDirection_ = calculateDirection(isLeftDominant);
                }
                break;

            case Contact:
                if (dominantVel < IDLE_THRESHOLD) {
                    currentPhase_ = FollowThrough;
                    finalizeKick();
                }
                break;

            case FollowThrough:
                if (dominantVel < RESET_THRESHOLD) {
                    currentPhase_ = Idle;
                    onKickComplete_(lastKick_);
                }
                break;
        }
    }

private:
    static constexpr float WINDUP_THRESHOLD = 0.5f;       // m/s
    static constexpr float ACCELERATION_THRESHOLD = 2.0f; // m/s
    static constexpr float IDLE_THRESHOLD = 0.3f;         // m/s
    static constexpr float RESET_THRESHOLD = 0.1f;        // m/s
};
```

## Appendix: Virtual Goal Zones

```
+-------+-------+-------+
| TL(3) | TC(2) | TR(3) |  <- Top row (hardest, 3x multiplier)
+-------+-------+-------+
| ML(2) | MC(1) | MR(2) |  <- Middle row (medium)
+-------+-------+-------+
| BL(2) | BC(1) | BR(2) |  <- Bottom row (easier)
+-------+-------+-------+

Zone scoring:
- Corners (TL, TR): 300 points (3x)
- Edges (TC, ML, MR, BL, BR): 200 points (2x)
- Center (MC, BC): 100 points (1x)
```

---

*Plan created for kinect-football v1.0*
*Leveraging patterns from kinect-native clinical body tracking application*

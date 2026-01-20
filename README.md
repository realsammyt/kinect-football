# Kinect Football - FIFA 2026 Soccer Simulator

A kiosk-style soccer simulator using Azure Kinect for body tracking and kick detection. Designed for interactive installations at events, retail locations, and exhibitions.

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## Features

- **Real-time Body Tracking** - Full skeleton tracking using Azure Kinect Body Tracking SDK
- **Kick Detection** - Detects and analyzes soccer kicks with speed and direction
- **Header Detection** - Tracks head movement for header challenges
- **Multiple Game Modes**
  - Accuracy Challenge - Hit target zones in the goal
  - Power Challenge - Kick with maximum power
  - Penalty Shootout - Classic penalty kick gameplay
- **Kiosk Mode** - Automated session management for public installations
- **Demo Mode** - Runs without Kinect hardware for testing/demos
- **FIFA 2026 Theme** - Custom visual theme with branded colors

## Screenshots

The application renders in portrait mode (1080x1920) optimized for kiosk displays:

- Attract screen with animated elements
- Real-time skeleton visualization with glow effects
- Goal with 3D posts, net pattern, and animated crosshair targets
- Score panel with time countdown

## Requirements

### Hardware
- Windows 10/11 (64-bit)
- Azure Kinect DK sensor
- NVIDIA GPU with CUDA support (for body tracking)
- Display: 1080x1920 portrait recommended

### Software Dependencies
- [Azure Kinect SDK v1.4.2](https://github.com/microsoft/Azure-Kinect-Sensor-SDK/releases)
- [Azure Kinect Body Tracking SDK](https://docs.microsoft.com/en-us/azure/kinect-dk/body-sdk-download)
- Visual Studio 2019/2022 with C++ workload
- CMake 3.15+
- DirectX 11

### Optional
- OpenCV 4.x (for advanced game rendering)

## Building

### 1. Install Dependencies

Download and install both Azure Kinect SDKs to their default locations:
- `C:\Program Files\Azure Kinect SDK v1.4.2`
- `C:\Program Files\Azure Kinect Body Tracking SDK`

### 2. Clone and Build

```bash
git clone https://github.com/realsammyt/kinect-football.git
cd kinect-football

mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### 3. Run

```bash
# With Kinect connected
./bin/Release/KinectFootball.exe

# Demo mode (no Kinect required)
./bin/Release/KinectFootball.exe --demo
```

## Project Structure

```
kinect-football/
├── include/              # Header files
│   ├── VectorMath.h      # Shared vector math utilities
│   └── UITheme.h         # FIFA 2026 visual theme constants
├── src/
│   ├── core/             # Kinect device and body tracking
│   │   ├── KinectDevice.cpp
│   │   ├── BodyTracker.cpp
│   │   └── PlayerTracker.cpp
│   ├── motion/           # Motion detection algorithms
│   │   ├── MotionHistory.cpp
│   │   ├── KickDetector.cpp
│   │   ├── KickAnalyzer.cpp
│   │   └── HeaderDetector.cpp
│   ├── game/             # Game modes and challenges
│   │   ├── GameManager.cpp
│   │   ├── AccuracyChallenge.cpp
│   │   ├── PowerChallenge.cpp
│   │   ├── PenaltyShootout.cpp
│   │   └── ScoringEngine.cpp
│   ├── gui/              # ImGui/DirectX rendering
│   │   └── Application.cpp
│   ├── kiosk/            # Kiosk session management
│   │   ├── KioskManager.cpp
│   │   └── SessionManager.cpp
│   └── main.cpp          # Entry point
├── .claude/              # Development workflow state
│   ├── plans/
│   └── designs/
├── CMakeLists.txt
└── README.md
```

## Configuration

The application can be configured via command-line arguments:

| Argument | Description |
|----------|-------------|
| `--demo` | Run in demo mode without Kinect hardware |
| `--fullscreen` | Launch in fullscreen mode |
| `--width <n>` | Set window width (default: 1080) |
| `--height <n>` | Set window height (default: 1920) |

## Architecture

### Core Components

- **KinectDevice** - Manages Azure Kinect sensor initialization and frame capture
- **BodyTracker** - Processes body tracking frames and extracts skeleton data
- **PlayerTracker** - Identifies and tracks individual players across frames

### Motion Detection

- **MotionHistory** - Circular buffer storing joint position history
- **KickDetector** - State machine detecting kick wind-up, strike, and follow-through
- **KickAnalyzer** - Calculates kick power, direction, and accuracy
- **HeaderDetector** - Detects head movement for header challenges

### Game System

- **GameManager** - Coordinates game state and challenge flow
- **ChallengeBase** - Abstract base class for game challenges
- **ScoringEngine** - Calculates and manages scores

### Kiosk System

- **KioskManager** - Manages attract mode, session flow, and idle timeouts
- **SessionManager** - Thread-safe session state with analytics tracking

## Visual Theme

The application uses a FIFA 2026-inspired color palette:

| Color | Hex | Usage |
|-------|-----|-------|
| Navy | `#0A1628` | Background |
| Teal | `#00D4AA` | Primary accent, buttons |
| Gold | `#FFD700` | Score, highlights |
| Coral | `#FF6B6B` | Warnings, errors |
| Orange | `#FF9F43` | Secondary accent |

## Version History

### v1.2.0 (Current)
- Added intro options screen with player customization
- 3 jersey color options (Teal, Coral, Gold)
- 2 background themes (Day/Night)
- Live preview of selections
- 30-second auto-continue timeout

### v1.1.0
- Fixed deadlock potential in SessionManager
- Added DirectX error handling
- Implemented FIFA 2026 visual theme
- Enhanced goal visualization with 3D posts and net
- Added skeleton glow effects
- Redesigned score panel
- Created shared VectorMath and UITheme utilities

### v1.0.0
- Initial release
- Core body tracking and kick detection
- Basic game challenges
- Kiosk mode with session management
- Demo mode support

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- [Azure Kinect SDK](https://github.com/microsoft/Azure-Kinect-Sensor-SDK)
- [Dear ImGui](https://github.com/ocornut/imgui)
- FIFA 2026 World Cup branding inspiration

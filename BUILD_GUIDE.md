# Kinect Football - Build Guide

Complete instructions for building and running the game challenge system.

## Prerequisites

### Required Software

1. **Visual Studio 2019 or later** (Windows)
   - C++ desktop development workload
   - CMake tools

2. **Azure Kinect SDK 1.4.1+**
   - Download: https://github.com/microsoft/Azure-Kinect-Sensor-SDK/releases
   - Install both Sensor SDK and Body Tracking SDK

3. **OpenCV 4.5.0+**
   - Download: https://opencv.org/releases/
   - Or install via vcpkg: `vcpkg install opencv4`

4. **CMake 3.15+**
   - Download: https://cmake.org/download/

### Hardware

- **Azure Kinect DK** camera
- **NVIDIA GPU** with CUDA support (recommended for body tracking)
- **USB 3.0** port

## Build Steps

### Option 1: Visual Studio (Windows)

1. **Clone repository**
```bash
git clone https://github.com/your-org/kinect-football.git
cd kinect-football
```

2. **Install Azure Kinect SDK**
```bash
# Download and install from:
# https://github.com/microsoft/Azure-Kinect-Sensor-SDK/releases
# Install both:
# - Azure Kinect Sensor SDK
# - Azure Kinect Body Tracking SDK
```

3. **Configure environment variables**
```bash
# Add to system PATH:
C:\Program Files\Azure Kinect SDK v1.4.1\sdk\windows-desktop\amd64\release\bin
C:\Program Files\Azure Kinect Body Tracking SDK\sdk\windows-desktop\amd64\release\bin

# Set environment variables:
set K4A_ROOT=C:\Program Files\Azure Kinect SDK v1.4.1
set K4ABT_ROOT=C:\Program Files\Azure Kinect Body Tracking SDK
set OpenCV_DIR=C:\path\to\opencv\build
```

4. **Generate Visual Studio solution**
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
```

5. **Build project**
```bash
# Open in Visual Studio
start KinectFootballGame.sln

# Or build from command line
cmake --build . --config Release
```

6. **Run example**
```bash
cd Release
game_example.exe accuracy
```

### Option 2: CMake + Ninja (Cross-platform)

1. **Install dependencies**
```bash
# Using vcpkg
vcpkg install azure-kinect-sensor-sdk
vcpkg install opencv4
```

2. **Configure build**
```bash
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

3. **Build**
```bash
ninja
```

4. **Run**
```bash
./game_example accuracy
```

### Option 3: Docker (Linux with GPU)

```dockerfile
FROM nvidia/cuda:11.4.0-cudnn8-runtime-ubuntu20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libopencv-dev \
    wget

# Install Azure Kinect SDK
RUN wget https://packages.microsoft.com/ubuntu/18.04/prod/pool/main/libk/libk4a1.4/libk4a1.4_1.4.1_amd64.deb
RUN wget https://packages.microsoft.com/ubuntu/18.04/prod/pool/main/libk/libk4a1.4-dev/libk4a1.4-dev_1.4.1_amd64.deb
RUN wget https://packages.microsoft.com/ubuntu/18.04/prod/pool/main/libk/libk4abt1.1/libk4abt1.1_1.1.0_amd64.deb
RUN dpkg -i *.deb

# Copy source
COPY . /app
WORKDIR /app

# Build
RUN mkdir build && cd build && cmake .. && make

CMD ["./build/game_example", "accuracy"]
```

## Project Structure

```
kinect-football/
├── include/
│   └── GameConfig.h              # Configuration header
├── src/
│   └── game/
│       ├── ChallengeBase.h/cpp   # Base class
│       ├── AccuracyChallenge.h/cpp
│       ├── PowerChallenge.h/cpp
│       ├── PenaltyShootout.h/cpp
│       ├── ScoringEngine.h/cpp
│       ├── GameManager.h/cpp
│       ├── CMakeLists.txt        # Build config
│       └── README.md
├── examples/
│   └── game_example.cpp          # Example application
├── assets/                       # Resources
│   └── achievements/
├── build/                        # Generated (gitignored)
└── BUILD_GUIDE.md               # This file
```

## CMake Options

Configure build with these options:

```bash
cmake .. \
  -DBUILD_GAME_EXAMPLE=ON \           # Build example app
  -DCMAKE_BUILD_TYPE=Release \        # Release build
  -DK4A_ROOT=/path/to/k4a \          # Kinect SDK path
  -DOpenCV_DIR=/path/to/opencv        # OpenCV path
```

## Troubleshooting

### Issue: "k4a.dll not found"

**Solution:** Add Azure Kinect SDK bin directory to PATH:
```bash
set PATH=%PATH%;C:\Program Files\Azure Kinect SDK v1.4.1\sdk\windows-desktop\amd64\release\bin
```

### Issue: "Body tracking model not found"

**Solution:** Copy body tracking models to executable directory:
```bash
copy "C:\Program Files\Azure Kinect Body Tracking SDK\sdk\windows-desktop\amd64\release\bin\dnn_model_2_0_op11.onnx" Release\
copy "C:\Program Files\Azure Kinect Body Tracking SDK\sdk\windows-desktop\amd64\release\bin\cudnn64_8.dll" Release\
```

### Issue: "OpenCV DLLs missing"

**Solution:** Copy OpenCV DLLs to output directory:
```bash
copy C:\path\to\opencv\build\x64\vc16\bin\opencv_world*.dll Release\
```

### Issue: "Kinect not detected"

**Checks:**
1. Device connected to USB 3.0 port
2. Kinect firmware updated
3. Drivers installed correctly
4. Run `AzureKinectViewer.exe` to test

### Issue: "Body tracking slow/freezing"

**Solutions:**
1. Ensure NVIDIA GPU with CUDA is available
2. Update GPU drivers
3. Use NFOV depth mode (not WFOV)
4. Reduce camera FPS to 15 if needed

### Issue: "CMake can't find Azure Kinect"

**Solution:** Manually specify paths:
```bash
cmake .. \
  -DK4A_ROOT="C:/Program Files/Azure Kinect SDK v1.4.1" \
  -DK4ABT_ROOT="C:/Program Files/Azure Kinect Body Tracking SDK"
```

## Running Challenges

### Accuracy Challenge
```bash
game_example accuracy
# Or: game_example 1
```

Hit target zones in 3x3 grid. Corners worth 3x points!

### Power Challenge
```bash
game_example power
# Or: game_example 2
```

Kick as hard as possible. 3 attempts to show max power.

### Penalty Shootout
```bash
game_example penalty
# Or: game_example 3
```

Score penalties against AI goalkeeper. Best of 5!

## Configuration

Edit configurations in `GameConfig.h`:

### Easy Mode
```cpp
config.accuracyConfig.timeLimitSeconds = 90.0f;
config.accuracyConfig.minimumAccuracyForPass = 0.3f;
config.penaltyConfig.goalkeeperCoverage = 0.4f;
config.penaltyConfig.goalkeeperReactionTime = 0.5f;
```

### Hard Mode
```cpp
config.accuracyConfig.timeLimitSeconds = 45.0f;
config.accuracyConfig.minimumAccuracyForPass = 0.7f;
config.penaltyConfig.goalkeeperCoverage = 0.9f;
config.penaltyConfig.goalkeeperReactionTime = 0.2f;
```

### Custom Scoring
```cpp
config.accuracyConfig.scoring.basePoints = 200;
config.accuracyConfig.scoring.accuracyMultiplier = 2.0f;
config.powerConfig.pointsPerKmh = 15;
```

## Integration into Existing Project

### As a Library

1. **Add to your CMakeLists.txt:**
```cmake
add_subdirectory(src/game)
target_link_libraries(your_app PRIVATE kinect_game)
```

2. **Include headers:**
```cpp
#include "GameManager.h"
```

3. **Use in your code:**
```cpp
GameManager manager;
manager.initialize();
manager.startChallenge(ChallengeType::ACCURACY);

// In game loop:
manager.processFrame(skeleton, depth, deltaTime);
manager.render(frame);
```

### As Static Library

1. **Build library:**
```bash
cmake --build . --target kinect_game
```

2. **Link in your project:**
```cmake
find_package(KinectGame REQUIRED)
target_link_libraries(your_app PRIVATE KinectGame::kinect_game)
```

## Performance Optimization

### Debug vs Release

Always use **Release** build for performance:
```bash
cmake --build . --config Release
```

Debug builds are ~10x slower due to body tracking overhead.

### Body Tracking Performance

```cpp
// Use fastest depth mode
config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;

// Reduce FPS if needed
config.camera_fps = K4A_FRAMES_PER_SECOND_15;

// Use GPU acceleration
k4abt_tracker_configuration_t trackerConfig = K4ABT_TRACKER_CONFIG_DEFAULT;
trackerConfig.processing_mode = K4ABT_TRACKER_PROCESSING_MODE_GPU;
```

### Kick Detection Tuning

Edit thresholds in challenge implementations:
```cpp
// AccuracyChallenge.cpp
const float windupThreshold = 0.15f;  // Adjust sensitivity
const float impactThreshold = 0.2f;
```

## Deployment

### Standalone Executable

Create portable package:
```bash
# Copy executable
copy Release\game_example.exe deploy\

# Copy Azure Kinect DLLs
copy "%K4A_ROOT%\sdk\windows-desktop\amd64\release\bin\*.dll" deploy\
copy "%K4ABT_ROOT%\sdk\windows-desktop\amd64\release\bin\*.dll" deploy\
copy "%K4ABT_ROOT%\sdk\windows-desktop\amd64\release\bin\*.onnx" deploy\

# Copy OpenCV DLLs
copy "%OpenCV_DIR%\x64\vc16\bin\*.dll" deploy\

# Copy assets
xcopy /E /I assets deploy\assets
```

### Installer (NSIS)

```nsis
; installer.nsi
Name "Kinect Football"
OutFile "KinectFootball_Setup.exe"
InstallDir "$PROGRAMFILES64\KinectFootball"

Section "Install"
    SetOutPath $INSTDIR
    File /r "deploy\*.*"
    CreateShortcut "$DESKTOP\Kinect Football.lnk" "$INSTDIR\game_example.exe"
SectionEnd
```

## Testing

### Unit Tests

Build with testing enabled:
```bash
cmake .. -DBUILD_TESTING=ON
ctest --output-on-failure
```

### Manual Test Checklist

- [ ] Kinect detected and streaming
- [ ] Body tracking working
- [ ] Accuracy challenge completes
- [ ] Power challenge records velocity
- [ ] Penalty shootout goalkeeper responds
- [ ] Achievements unlock
- [ ] Session stats calculated
- [ ] No crashes on edge cases

### Performance Benchmarks

Target metrics:
- **Frame rate**: 30 fps constant
- **Kick detection latency**: < 100ms
- **Memory usage**: < 500 MB
- **Body tracking**: < 50ms per frame

## Support

### Documentation
- Full API docs: `src/game/README.md`
- Examples: `examples/`
- Configuration: `include/GameConfig.h`

### Known Issues
- Windows only (Linux support planned)
- Requires NVIDIA GPU for body tracking
- Single player only (multiplayer planned)

### Contact
- GitHub Issues: https://github.com/your-org/kinect-football/issues
- Email: support@your-org.com

---

**Build Status**: ✅ Production-ready
**Version**: 1.0.0
**Last Updated**: 2026-01-18

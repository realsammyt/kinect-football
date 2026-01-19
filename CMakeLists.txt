cmake_minimum_required(VERSION 3.15)
project(KinectFootball VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(ENABLE_AUDIO "Enable audio system" ON)
option(ENABLE_SOCIAL "Enable social sharing features" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# =============================================================================
# Azure Kinect SDK
# =============================================================================
set(K4A_SDK_PATH "C:/Program Files/Azure Kinect SDK v1.4.2" CACHE PATH "Azure Kinect SDK path")

if(EXISTS "${K4A_SDK_PATH}")
    set(K4A_INCLUDE_DIR "${K4A_SDK_PATH}/sdk/include")
    set(K4A_LIB_DIR "${K4A_SDK_PATH}/sdk/windows-desktop/amd64/release/lib")
    set(K4A_BIN_DIR "${K4A_SDK_PATH}/sdk/windows-desktop/amd64/release/bin")

    find_library(K4A_LIBRARY k4a PATHS ${K4A_LIB_DIR} REQUIRED)
    find_library(K4ARECORD_LIBRARY k4arecord PATHS ${K4A_LIB_DIR})

    message(STATUS "Found Azure Kinect SDK: ${K4A_SDK_PATH}")
else()
    message(FATAL_ERROR "Azure Kinect SDK not found at ${K4A_SDK_PATH}")
endif()

# =============================================================================
# Azure Kinect Body Tracking SDK
# =============================================================================
set(K4ABT_SDK_PATH "C:/Program Files/Azure Kinect Body Tracking SDK" CACHE PATH "Azure Kinect Body Tracking SDK path")

if(EXISTS "${K4ABT_SDK_PATH}")
    set(K4ABT_INCLUDE_DIR "${K4ABT_SDK_PATH}/sdk/include")
    set(K4ABT_LIB_DIR "${K4ABT_SDK_PATH}/sdk/windows-desktop/amd64/release/lib")
    set(K4ABT_BIN_DIR "${K4ABT_SDK_PATH}/sdk/windows-desktop/amd64/release/bin")

    find_library(K4ABT_LIBRARY k4abt PATHS ${K4ABT_LIB_DIR} REQUIRED)

    message(STATUS "Found Azure Kinect Body Tracking SDK: ${K4ABT_SDK_PATH}")
else()
    message(FATAL_ERROR "Azure Kinect Body Tracking SDK not found at ${K4ABT_SDK_PATH}")
endif()

# =============================================================================
# OpenCV (optional, for game rendering)
# =============================================================================
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV: ${OpenCV_DIR}")
else()
    message(WARNING "OpenCV not found - game module rendering will be disabled")
endif()

# =============================================================================
# Dear ImGui (fetch from GitHub)
# =============================================================================
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.1
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# =============================================================================
# Source Files
# =============================================================================
set(CORE_SOURCES
    src/core/KinectDevice.cpp
    src/core/BodyTracker.cpp
    src/core/PlayerTracker.cpp
)

set(MOTION_SOURCES
    src/motion/MotionHistory.cpp
    src/motion/KickDetector.cpp
    src/motion/KickAnalyzer.cpp
    src/motion/HeaderDetector.cpp
)

# Game sources require OpenCV for rendering
if(OpenCV_FOUND)
    set(GAME_SOURCES
        src/game/ChallengeBase.cpp
        src/game/GameManager.cpp
        src/game/AccuracyChallenge.cpp
        src/game/PowerChallenge.cpp
        src/game/PenaltyShootout.cpp
        src/game/ScoringEngine.cpp
    )
else()
    set(GAME_SOURCES "")
    message(STATUS "OpenCV not found - game module disabled")
endif()

set(GUI_SOURCES
    src/gui/Application.cpp
)

set(KIOSK_SOURCES
    src/kiosk/KioskManager.cpp
    src/kiosk/SessionManager.cpp
)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(KinectFootball WIN32
    src/main.cpp
    ${CORE_SOURCES}
    ${MOTION_SOURCES}
    ${GAME_SOURCES}
    ${GUI_SOURCES}
    ${KIOSK_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(KinectFootball PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${K4A_INCLUDE_DIR}
    ${K4ABT_INCLUDE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(KinectFootball PRIVATE
    ${K4A_LIBRARY}
    ${K4ABT_LIBRARY}
    d3d11
    dxgi
    d3dcompiler
)

if(OpenCV_FOUND)
    target_link_libraries(KinectFootball PRIVATE ${OpenCV_LIBS})
    target_include_directories(KinectFootball PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_compile_definitions(KinectFootball PRIVATE HAVE_OPENCV)
endif()

# Windows-specific settings
if(WIN32)
    target_compile_definitions(KinectFootball PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )

    # Use MultiThreaded DLL runtime
    set_property(TARGET KinectFootball PROPERTY MSVC_RUNTIME_LIBRARY
                 "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# =============================================================================
# Copy DLLs post-build
# =============================================================================
set(K4A_DLLS
    "${K4A_BIN_DIR}/k4a.dll"
    "${K4A_BIN_DIR}/depthengine_2_0.dll"
)

set(K4ABT_DLLS
    "${K4ABT_BIN_DIR}/k4abt.dll"
    "${K4ABT_BIN_DIR}/onnxruntime.dll"
    "${K4ABT_BIN_DIR}/directml.dll"
    "${K4ABT_BIN_DIR}/dnn_model_2_0_op11.onnx"
)

foreach(DLL ${K4A_DLLS} ${K4ABT_DLLS})
    if(EXISTS "${DLL}")
        add_custom_command(TARGET KinectFootball POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:KinectFootball>
            COMMENT "Copying ${DLL}"
        )
    endif()
endforeach()

# =============================================================================
# Installation
# =============================================================================
install(TARGETS KinectFootball
    RUNTIME DESTINATION bin
)

install(DIRECTORY assets/
    DESTINATION assets
)

message(STATUS "=== KinectFootball Configuration ===")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Audio: ${ENABLE_AUDIO}")
message(STATUS "  Social: ${ENABLE_SOCIAL}")
message(STATUS "====================================")

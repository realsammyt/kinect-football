# Kinect Football - Game Challenge System
# CMake build configuration

cmake_minimum_required(VERSION 3.15)

project(KinectFootballGame VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(k4a REQUIRED)
find_package(k4abt REQUIRED)
find_package(OpenCV REQUIRED)

# Game library sources
set(GAME_SOURCES
    ChallengeBase.cpp
    AccuracyChallenge.cpp
    PowerChallenge.cpp
    PenaltyShootout.cpp
    ScoringEngine.cpp
    GameManager.cpp
)

set(GAME_HEADERS
    ChallengeBase.h
    AccuracyChallenge.h
    PowerChallenge.h
    PenaltyShootout.h
    ScoringEngine.h
    GameManager.h
    ../../include/GameConfig.h
)

# Create game library
add_library(kinect_game STATIC ${GAME_SOURCES} ${GAME_HEADERS})

target_include_directories(kinect_game
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(kinect_game
    PUBLIC
        k4a::k4a
        k4abt::k4abt
    PRIVATE
        ${OpenCV_LIBS}
)

# Compiler warnings
if(MSVC)
    target_compile_options(kinect_game PRIVATE /W4)
else()
    target_compile_options(kinect_game PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optional: Build example
option(BUILD_GAME_EXAMPLE "Build game example application" ON)

if(BUILD_GAME_EXAMPLE)
    add_executable(game_example
        ../../examples/game_example.cpp
    )

    target_link_libraries(game_example
        PRIVATE
            kinect_game
            ${OpenCV_LIBS}
    )

    # Copy assets to build directory
    add_custom_command(TARGET game_example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:game_example>/assets
        COMMENT "Copying assets to build directory"
    )
endif()

# Installation
install(TARGETS kinect_game
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${GAME_HEADERS}
    DESTINATION include/kinect/game
)

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/KinectGameConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KinectGameConfig.cmake"
    INSTALL_DESTINATION lib/cmake/KinectGame
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/KinectGameConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/KinectGameConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/KinectGameConfigVersion.cmake"
    DESTINATION lib/cmake/KinectGame
)

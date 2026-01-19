#pragma once

#include "BodyTracker.h"
#include <map>
#include <functional>

namespace kinect {
namespace core {

/**
 * @brief Player zone assignment for multi-player games
 */
enum class PlayerZone {
    Unknown,
    Left,       // Left side of play area
    Right,      // Right side of play area
    Center      // Single player mode
};

/**
 * @brief Extended player data for game context
 */
struct PlayerData {
    uint32_t bodyId = 0;
    PlayerZone zone = PlayerZone::Unknown;
    BodyData body;

    // Tracking state
    int framesTracked = 0;
    int framesLost = 0;
    bool isConfirmed = false;  // True after stable tracking

    // Game state
    int playerNumber = 0;      // 1 or 2 for multiplayer
    bool isActive = false;
};

/**
 * @brief Multi-player tracking and zone management
 *
 * Manages player identification, zone assignment,
 * and stability tracking for kiosk gameplay.
 */
class PlayerTracker {
public:
    PlayerTracker();

    /**
     * @brief Process bodies and update player tracking
     * @param bodies Bodies from BodyTracker
     */
    void update(const std::vector<BodyData>& bodies);

    /**
     * @brief Get the primary player (closest to center)
     * @return Pointer to player data, or nullptr if no player
     */
    const PlayerData* getPrimaryPlayer() const;

    /**
     * @brief Get player by zone
     * @param zone The zone to query
     * @return Pointer to player data, or nullptr
     */
    const PlayerData* getPlayerInZone(PlayerZone zone) const;

    /**
     * @brief Get all tracked players
     */
    const std::map<uint32_t, PlayerData>& getPlayers() const { return players_; }

    /**
     * @brief Get number of active players
     */
    int getActivePlayerCount() const;

    /**
     * @brief Check if any player is detected
     */
    bool hasAnyPlayer() const { return !players_.empty(); }

    /**
     * @brief Set callback for player enter/exit
     */
    void setPlayerEnterCallback(std::function<void(const PlayerData&)> cb) {
        onPlayerEnter_ = cb;
    }
    void setPlayerExitCallback(std::function<void(const PlayerData&)> cb) {
        onPlayerExit_ = cb;
    }

    /**
     * @brief Configure play area boundaries
     * @param leftBoundary X position dividing left from center (mm)
     * @param rightBoundary X position dividing center from right (mm)
     */
    void setZoneBoundaries(float leftBoundary, float rightBoundary);

    /**
     * @brief Set minimum frames before player is "confirmed"
     */
    void setConfirmationThreshold(int frames) { confirmationThreshold_ = frames; }

    /**
     * @brief Set frames before lost player is removed
     */
    void setLostThreshold(int frames) { lostThreshold_ = frames; }

    /**
     * @brief Reset all player tracking
     */
    void reset();

private:
    std::map<uint32_t, PlayerData> players_;

    // Callbacks
    std::function<void(const PlayerData&)> onPlayerEnter_;
    std::function<void(const PlayerData&)> onPlayerExit_;

    // Zone configuration
    float leftBoundary_ = -500.0f;   // mm from center
    float rightBoundary_ = 500.0f;   // mm from center

    // Tracking thresholds
    int confirmationThreshold_ = 10;  // Frames to confirm player
    int lostThreshold_ = 30;          // Frames before removal

    PlayerZone determineZone(const BodyData& body) const;
    void assignPlayerNumbers();
};

} // namespace core
} // namespace kinect

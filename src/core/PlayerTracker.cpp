#include "PlayerTracker.h"
#include <algorithm>
#include <iostream>

namespace kinect {
namespace core {

PlayerTracker::PlayerTracker() = default;

void PlayerTracker::update(const std::vector<BodyData>& bodies) {
    // Mark all existing players as potentially lost
    for (auto& [id, player] : players_) {
        player.isActive = false;
    }

    // Update existing players and add new ones
    for (const auto& body : bodies) {
        auto it = players_.find(body.id);

        if (it != players_.end()) {
            // Update existing player
            PlayerData& player = it->second;
            player.body = body;
            player.isActive = true;
            player.framesTracked++;
            player.framesLost = 0;
            player.zone = determineZone(body);

            // Check for confirmation
            if (!player.isConfirmed && player.framesTracked >= confirmationThreshold_) {
                player.isConfirmed = true;
                if (onPlayerEnter_) {
                    onPlayerEnter_(player);
                }
            }
        } else {
            // New player detected
            PlayerData newPlayer;
            newPlayer.bodyId = body.id;
            newPlayer.body = body;
            newPlayer.zone = determineZone(body);
            newPlayer.framesTracked = 1;
            newPlayer.framesLost = 0;
            newPlayer.isActive = true;
            newPlayer.isConfirmed = false;

            players_[body.id] = newPlayer;
        }
    }

    // Increment lost counter for missing players
    std::vector<uint32_t> toRemove;
    for (auto& [id, player] : players_) {
        if (!player.isActive) {
            player.framesLost++;
            if (player.framesLost >= lostThreshold_) {
                toRemove.push_back(id);
                if (player.isConfirmed && onPlayerExit_) {
                    onPlayerExit_(player);
                }
            }
        }
    }

    // Remove lost players
    for (uint32_t id : toRemove) {
        players_.erase(id);
    }

    // Assign player numbers
    assignPlayerNumbers();
}

const PlayerData* PlayerTracker::getPrimaryPlayer() const {
    if (players_.empty()) {
        return nullptr;
    }

    // Find closest player to center (smallest absolute X)
    const PlayerData* primary = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (const auto& [id, player] : players_) {
        if (!player.isConfirmed || !player.isActive) {
            continue;
        }

        // Use pelvis position for distance
        float pelvisX = player.body.joints[K4ABT_JOINT_PELVIS].position.xyz.x;
        float distance = std::abs(pelvisX);

        if (distance < minDistance) {
            minDistance = distance;
            primary = &player;
        }
    }

    return primary;
}

const PlayerData* PlayerTracker::getPlayerInZone(PlayerZone zone) const {
    for (const auto& [id, player] : players_) {
        if (player.isConfirmed && player.isActive && player.zone == zone) {
            return &player;
        }
    }
    return nullptr;
}

int PlayerTracker::getActivePlayerCount() const {
    int count = 0;
    for (const auto& [id, player] : players_) {
        if (player.isConfirmed && player.isActive) {
            count++;
        }
    }
    return count;
}

void PlayerTracker::setZoneBoundaries(float leftBoundary, float rightBoundary) {
    leftBoundary_ = leftBoundary;
    rightBoundary_ = rightBoundary;
}

void PlayerTracker::reset() {
    players_.clear();
}

PlayerZone PlayerTracker::determineZone(const BodyData& body) const {
    // Use pelvis X position for zone determination
    float pelvisX = body.joints[K4ABT_JOINT_PELVIS].position.xyz.x;

    if (pelvisX < leftBoundary_) {
        return PlayerZone::Left;
    } else if (pelvisX > rightBoundary_) {
        return PlayerZone::Right;
    } else {
        return PlayerZone::Center;
    }
}

void PlayerTracker::assignPlayerNumbers() {
    // Collect confirmed active players
    std::vector<PlayerData*> activePlayers;
    for (auto& [id, player] : players_) {
        if (player.isConfirmed && player.isActive) {
            activePlayers.push_back(&player);
        }
    }

    // Sort by X position (left to right)
    std::sort(activePlayers.begin(), activePlayers.end(),
              [](const PlayerData* a, const PlayerData* b) {
                  return a->body.joints[K4ABT_JOINT_PELVIS].position.xyz.x <
                         b->body.joints[K4ABT_JOINT_PELVIS].position.xyz.x;
              });

    // Assign player numbers
    for (size_t i = 0; i < activePlayers.size(); i++) {
        activePlayers[i]->playerNumber = static_cast<int>(i + 1);
    }
}

} // namespace core
} // namespace kinect

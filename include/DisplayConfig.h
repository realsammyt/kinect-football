#pragma once

#include <cstdint>

namespace kinect {
namespace config {

/**
 * @brief Display orientation modes
 */
enum class DisplayOrientation {
    Landscape,      // Standard 16:9 horizontal (1920x1080)
    Portrait        // Vertical kiosk mode (1080x1920)
};

/**
 * @brief Display configuration for portrait kiosk
 *
 * The kiosk uses a portrait-oriented touchscreen with an external
 * LED scoreboard mounted above. Layout is optimized for vertical viewing.
 */
struct DisplayConfig {
    // Resolution (portrait mode default)
    int width = 1080;
    int height = 1920;
    DisplayOrientation orientation = DisplayOrientation::Portrait;

    // Safe zones (accounting for kiosk bezel and branding area)
    int topMargin = 60;       // Space for status bar
    int bottomMargin = 200;   // Space for team branding/logo
    int sideMargin = 40;

    // UI element sizes (scaled for portrait viewing distance)
    int headerHeight = 120;
    int footerHeight = 100;
    int buttonHeight = 80;
    int fontSize = 32;
    int largeFontSize = 64;

    // Touch target minimum (for accessibility)
    int minTouchTarget = 48;  // 48px minimum touch target

    // Portrait layout zones (from top to bottom)
    struct LayoutZones {
        // Top zone: Goal visualization / game area
        int gameAreaTop = 60;
        int gameAreaHeight = 800;

        // Middle zone: Score, power meter, feedback
        int feedbackTop = 880;
        int feedbackHeight = 400;

        // Bottom zone: Controls, instructions
        int controlsTop = 1300;
        int controlsHeight = 420;
    } zones;

    // External LED scoreboard integration
    struct ExternalScoreboard {
        bool enabled = true;
        int comPort = 3;           // Serial COM port
        int baudRate = 9600;
        // Format: "000 00 00 00" = Score, Time (MM:SS), Period/Round
    } scoreboard;

    // Calculate usable area
    int getUsableWidth() const { return width - (2 * sideMargin); }
    int getUsableHeight() const { return height - topMargin - bottomMargin; }

    // Check if point is in touch-safe area
    bool isInSafeArea(int x, int y) const {
        return x >= sideMargin && x <= (width - sideMargin) &&
               y >= topMargin && y <= (height - bottomMargin);
    }
};

/**
 * @brief Portrait-optimized goal layout
 *
 * Virtual goal displayed at top of screen in portrait mode.
 * 3x3 grid for target zones.
 *
 *  +-------+-------+-------+
 *  | TL(3) | TC(2) | TR(3) |  <- Corners hardest
 *  +-------+-------+-------+
 *  | ML(2) | MC(1) | MR(2) |  <- Middle easier
 *  +-------+-------+-------+
 *  | BL(2) | BC(1) | BR(2) |  <- Bottom row
 *  +-------+-------+-------+
 */
struct GoalLayout {
    // Goal dimensions (in portrait view, goal is wide relative to screen)
    int goalWidth = 900;      // Almost full width
    int goalHeight = 500;     // Shorter in portrait
    int goalX = 90;           // Centered with margins
    int goalY = 150;          // Below status bar

    // 3x3 target grid
    int gridCols = 3;
    int gridRows = 3;

    // Zone multipliers
    float cornerMultiplier = 3.0f;    // TL, TR
    float edgeMultiplier = 2.0f;      // TC, ML, MR, BL, BR
    float centerMultiplier = 1.0f;    // MC, BC

    // Calculate zone bounds
    int getZoneWidth() const { return goalWidth / gridCols; }
    int getZoneHeight() const { return goalHeight / gridRows; }

    int getZoneX(int col) const { return goalX + (col * getZoneWidth()); }
    int getZoneY(int row) const { return goalY + (row * getZoneHeight()); }
};

/**
 * @brief Power meter layout for portrait display
 */
struct PowerMeterLayout {
    // Vertical power meter on side of screen
    int x = 50;
    int y = 900;
    int width = 60;
    int height = 350;
    bool vertical = true;  // Vertical bar for portrait mode

    // Color gradient (bottom to top)
    uint32_t lowColor = 0xFF00FF00;     // Green
    uint32_t midColor = 0xFFFFFF00;     // Yellow
    uint32_t highColor = 0xFFFF0000;    // Red
};

/**
 * @brief Player silhouette position for portrait mode
 */
struct PlayerVisualization {
    // Player shown in bottom third of screen
    int centerX = 540;    // Center of 1080 width
    int centerY = 1500;   // Lower portion
    int maxWidth = 400;   // Scale to fit
    int maxHeight = 500;

    // Skeleton rendering
    float jointRadius = 8.0f;
    float boneWidth = 4.0f;
    uint32_t skeletonColor = 0xFF00FFFF;  // Cyan
    uint32_t kickFootColor = 0xFFFF00FF;  // Magenta highlight
};

} // namespace config
} // namespace kinect

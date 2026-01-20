#pragma once
/**
 * @file UITheme.h
 * @brief FIFA 2026 visual theme constants
 */

#include <imgui.h>

namespace kinect {
namespace theme {

// FIFA 2026 Color Palette
namespace colors {
    // Backgrounds
    constexpr ImU32 NAVY_BG = IM_COL32(10, 22, 40, 255);        // #0A1628
    constexpr ImU32 DEEP_NAVY = IM_COL32(13, 27, 42, 255);      // #0D1B2A
    constexpr ImU32 PANEL_BG = IM_COL32(15, 25, 40, 240);

    // Primary accents
    constexpr ImU32 TEAL = IM_COL32(0, 212, 170, 255);          // #00D4AA
    constexpr ImU32 TEAL_HOVER = IM_COL32(0, 240, 190, 255);
    constexpr ImU32 TEAL_GLOW = IM_COL32(0, 212, 170, 80);

    // Secondary accents
    constexpr ImU32 GOLD = IM_COL32(255, 215, 0, 255);          // #FFD700
    constexpr ImU32 CORAL = IM_COL32(255, 107, 107, 255);       // #FF6B6B
    constexpr ImU32 ORANGE = IM_COL32(255, 159, 67, 255);       // #FF9F43

    // Text
    constexpr ImU32 WHITE = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 GRAY = IM_COL32(160, 170, 180, 255);
    constexpr ImU32 DARK_GRAY = IM_COL32(80, 90, 100, 200);

    // Borders
    constexpr ImU32 BORDER = IM_COL32(0, 212, 170, 150);
    constexpr ImU32 BORDER_SUBTLE = IM_COL32(61, 90, 128, 200);

    // Goal colors
    constexpr ImU32 GOAL_POST = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 GOAL_POST_SHADOW = IM_COL32(180, 180, 180, 255);
    constexpr ImU32 NET_LINE = IM_COL32(100, 120, 140, 60);
    constexpr ImU32 TARGET_HIGHLIGHT = IM_COL32(0, 255, 200, 255);

    // Skeleton colors
    constexpr ImU32 JOINT = IM_COL32(200, 220, 255, 255);
    constexpr ImU32 BONE = IM_COL32(150, 180, 220, 200);
    constexpr ImU32 KICK_FOOT = IM_COL32(255, 215, 0, 255);

    // Power meter zones
    constexpr ImU32 POWER_LOW = IM_COL32(0, 200, 100, 255);
    constexpr ImU32 POWER_MED = IM_COL32(255, 220, 0, 255);
    constexpr ImU32 POWER_HIGH = IM_COL32(255, 150, 0, 255);
    constexpr ImU32 POWER_MAX = IM_COL32(255, 50, 50, 255);
}

// Font sizes for kiosk visibility (at 3+ meters)
namespace fonts {
    constexpr float HERO = 120.0f;      // Main titles
    constexpr float TITLE = 56.0f;      // Screen titles
    constexpr float HEADING = 40.0f;    // Section headings
    constexpr float BODY = 32.0f;       // Body text, scores
    constexpr float LABEL = 24.0f;      // Labels, captions
    constexpr float SMALL = 18.0f;      // Fine print
}

// Animation timing (seconds)
namespace timing {
    constexpr float STATE_TRANSITION = 0.35f;
    constexpr float BUTTON_HOVER = 0.15f;
    constexpr float PULSE_SLOW = 2.0f;   // Gentle attention
    constexpr float PULSE_MEDIUM = 4.0f; // Normal feedback
    constexpr float PULSE_FAST = 6.0f;   // Urgent feedback
    constexpr float COUNTDOWN_GO = 0.8f; // "GO!" display time
}

// Layout constants
namespace layout {
    constexpr float CORNER_RADIUS = 12.0f;
    constexpr float PANEL_PADDING = 20.0f;
    constexpr float BUTTON_HEIGHT = 160.0f;
    constexpr float MIN_TOUCH_TARGET = 48.0f;
}

// Background clear color (RGBA float)
inline void getBackgroundColor(float* out) {
    out[0] = 10.0f / 255.0f;   // R
    out[1] = 22.0f / 255.0f;   // G
    out[2] = 40.0f / 255.0f;   // B
    out[3] = 1.0f;             // A
}

// Jersey color palette
namespace jerseys {
    constexpr ImU32 TEAL = IM_COL32(0, 212, 170, 255);      // #00D4AA
    constexpr ImU32 CORAL = IM_COL32(255, 107, 107, 255);   // #FF6B6B
    constexpr ImU32 GOLD = IM_COL32(255, 215, 0, 255);      // #FFD700

    // Glow variants (reduced alpha)
    constexpr ImU32 TEAL_GLOW = IM_COL32(0, 212, 170, 80);
    constexpr ImU32 CORAL_GLOW = IM_COL32(255, 107, 107, 80);
    constexpr ImU32 GOLD_GLOW = IM_COL32(255, 215, 0, 80);
}

// Background theme colors (RGBA float arrays for D3D clear)
namespace backgrounds {
    inline void getNightColor(float* out) {
        out[0] = 0.039f;  out[1] = 0.086f;  out[2] = 0.157f;  out[3] = 1.0f;
    }

    inline void getDayColor(float* out) {
        out[0] = 0.529f;  out[1] = 0.808f;  out[2] = 0.922f;  out[3] = 1.0f;
    }
}

} // namespace theme
} // namespace kinect

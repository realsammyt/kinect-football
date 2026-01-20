# Intro Options Screen - Implementation Plan

## Overview

Add a new customization screen between Player Detection and Challenge Selection that allows players to:
1. Choose from 3 football jersey colors
2. Select Day or Night background theme

## Current Flow
```
Attract → PlayerDetected (2s) → SelectingChallenge → Countdown → Playing → Results
```

## New Flow
```
Attract → PlayerDetected (2s) → SelectingOptions → SelectingChallenge → Countdown → Playing → Results
                                      ↑
                                 NEW SCREEN
```

---

## Phase 1: Data Structures

### 1.1 Add Enums to common.h

```cpp
enum class JerseyColor {
    TEAL,      // Default - #00D4AA
    CORAL,     // Red/Pink - #FF6B6B
    GOLD       // Yellow/Gold - #FFD700
};

enum class BackgroundTheme {
    NIGHT,     // Dark navy - #0A1628 (default)
    DAY        // Light sky - #87CEEB
};
```

### 1.2 Extend SessionData in common.h

Add fields:
```cpp
JerseyColor selectedJersey = JerseyColor::TEAL;
BackgroundTheme selectedBackground = BackgroundTheme::NIGHT;
```

---

## Phase 2: UI Theme Extensions

### 2.1 Add to UITheme.h

```cpp
namespace jerseys {
    constexpr ImU32 TEAL = IM_COL32(0, 212, 170, 255);
    constexpr ImU32 CORAL = IM_COL32(255, 107, 107, 255);
    constexpr ImU32 GOLD = IM_COL32(255, 215, 0, 255);
}

namespace backgrounds {
    // Night (existing)
    constexpr float NIGHT[4] = { 0.039f, 0.086f, 0.157f, 1.0f };
    // Day
    constexpr float DAY[4] = { 0.529f, 0.808f, 0.922f, 1.0f };  // #87CEEB
}
```

---

## Phase 3: State Machine Extension

### 3.1 Add GameState in Application.h

```cpp
enum class GameState {
    Attract,
    PlayerDetected,
    SelectingOptions,      // NEW
    SelectingChallenge,
    Countdown,
    Playing,
    Results,
    Celebration,
    Error
};
```

### 3.2 Add Member Variables in Application.h

```cpp
// Player customization selections
JerseyColor selectedJersey_ = JerseyColor::TEAL;
BackgroundTheme selectedBackground_ = BackgroundTheme::NIGHT;

// Helper methods
void renderOptionsSelect();
ImU32 getJerseyColor(JerseyColor jersey) const;
void getBackgroundColor(BackgroundTheme theme, float* out) const;
```

---

## Phase 4: SessionManager Updates

### 4.1 Add Method to SessionManager.h

```cpp
void setPlayerOptions(const std::string& sessionId,
                      JerseyColor jersey,
                      BackgroundTheme background);
```

### 4.2 Implement in SessionManager.cpp

Store options in SessionData map.

---

## Phase 5: Application.cpp Implementation

### 5.1 Update render() Switch

Add case for `GameState::SelectingOptions`:
```cpp
case GameState::SelectingOptions:
    renderOptionsSelect();
    break;
```

### 5.2 Update State Transitions

In `updateStateLogic()`:
- PlayerDetected → SelectingOptions (2s auto)
- SelectingOptions → SelectingChallenge (button click OR 30s timeout)

### 5.3 Create renderOptionsSelect()

UI Layout (1080x1920 portrait):
```
┌─────────────────────────────────────┐
│      CUSTOMIZE YOUR PLAYER          │  y=150
├─────────────────────────────────────┤
│                                     │
│    SELECT JERSEY COLOR              │  y=300
│                                     │
│  ┌────────┐ ┌────────┐ ┌────────┐  │  y=400
│  │  TEAL  │ │ CORAL  │ │  GOLD  │  │  (280x120 buttons)
│  └────────┘ └────────┘ └────────┘  │
│                                     │
│    [Preview skeleton with color]    │  y=600-900
│                                     │
├─────────────────────────────────────┤
│                                     │
│    SELECT TIME OF DAY               │  y=1000
│                                     │
│  ┌──────────────┐ ┌──────────────┐  │  y=1100
│  │   ☀️ DAY     │ │   🌙 NIGHT   │  │  (400x120 buttons)
│  └──────────────┘ └──────────────┘  │
│                                     │
│    [Background preview]             │  y=1300-1500
│                                     │
├─────────────────────────────────────┤
│                                     │
│         [ CONTINUE → ]              │  y=1700 (500x100 button)
│                                     │
└─────────────────────────────────────┘
```

### 5.4 Apply Selections During Gameplay

- Modify `renderDemoSkeleton()` to use `selectedJersey_` color
- Modify `render()` clear color to use `selectedBackground_`

---

## Phase 6: Visual Effects

### 6.1 Jersey Button Styling
- Show colored rectangle preview in each button
- Highlight selected button with border
- Add hover effect

### 6.2 Background Toggle Styling
- Day button: Light blue background preview
- Night button: Dark navy background preview
- Highlight selected with teal border

### 6.3 Live Preview
- Render mini skeleton with selected jersey color
- Show background gradient preview

---

## Files to Modify

| File | Changes |
|------|---------|
| `include/common.h` | Add JerseyColor, BackgroundTheme enums; extend SessionData |
| `include/UITheme.h` | Add jersey colors and background color arrays |
| `src/gui/Application.h` | Add SelectingOptions state, member vars, methods |
| `src/gui/Application.cpp` | Add renderOptionsSelect(), update transitions, apply colors |
| `src/kiosk/SessionManager.h` | Add setPlayerOptions() method |
| `src/kiosk/SessionManager.cpp` | Implement setPlayerOptions() |

---

## Success Criteria

1. Player sees options screen after detection
2. Can select from 3 jersey colors with visual feedback
3. Can select Day or Night background
4. Preview updates in real-time
5. Selections persist through gameplay
6. Skeleton renders with selected jersey color
7. Background matches selected theme
8. 30s timeout auto-continues with defaults
9. Build passes with no warnings

---

## Estimated Scope

- Files modified: 6
- New methods: ~5
- New lines: ~400
- Risk: Low (UI-only changes, no core logic affected)

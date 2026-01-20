# Intro Options Screen - Implementation Design

## 1. Data Structures (common.h)

### 1.1 New Enums

```cpp
// Player jersey color options
enum class JerseyColor {
    TEAL = 0,      // Default - Team Teal
    CORAL = 1,     // Team Coral (Red/Pink)
    GOLD = 2       // Team Gold (Yellow)
};

// Background theme options
enum class BackgroundTheme {
    NIGHT = 0,     // Dark navy (default)
    DAY = 1        // Light sky blue
};
```

### 1.2 SessionData Extensions

```cpp
struct SessionData {
    // ... existing fields ...

    // Player customization options
    JerseyColor selectedJersey = JerseyColor::TEAL;
    BackgroundTheme selectedBackground = BackgroundTheme::NIGHT;

    // ... existing methods ...
};
```

---

## 2. Theme Extensions (UITheme.h)

### 2.1 Jersey Colors Namespace

```cpp
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
        out[0] = 0.039f;  out[1] = 0.086f;  out[2] = 0.157f;  out[3] = 1.0f;  // #0A1628
    }

    inline void getDayColor(float* out) {
        out[0] = 0.529f;  out[1] = 0.808f;  out[2] = 0.922f;  out[3] = 1.0f;  // #87CEEB
    }
}
```

---

## 3. Application.h Changes

### 3.1 GameState Enum Update

```cpp
enum class GameState {
    Attract,
    PlayerDetected,
    SelectingOptions,      // NEW: Jersey + Background selection
    SelectingChallenge,
    Countdown,
    Playing,
    Results,
    Celebration,
    Error
};
```

### 3.2 New Member Variables

```cpp
private:
    // Player customization state
    JerseyColor selectedJersey_ = JerseyColor::TEAL;
    BackgroundTheme selectedBackground_ = BackgroundTheme::NIGHT;
```

### 3.3 New Method Declarations

```cpp
private:
    // Options screen rendering
    void renderOptionsSelect();

    // Helper methods for customization
    ImU32 getJerseyColor(JerseyColor jersey) const;
    ImU32 getJerseyGlowColor(JerseyColor jersey) const;
    void getBackgroundClearColor(BackgroundTheme theme, float* out) const;

    // Mini skeleton preview for options screen
    void renderMiniSkeleton(ImDrawList* drawList, ImVec2 center, float scale, ImU32 color);
```

---

## 4. Application.cpp Implementation

### 4.1 Helper Methods

```cpp
ImU32 Application::getJerseyColor(JerseyColor jersey) const {
    switch (jersey) {
        case JerseyColor::TEAL:  return kinect::theme::jerseys::TEAL;
        case JerseyColor::CORAL: return kinect::theme::jerseys::CORAL;
        case JerseyColor::GOLD:  return kinect::theme::jerseys::GOLD;
        default: return kinect::theme::jerseys::TEAL;
    }
}

ImU32 Application::getJerseyGlowColor(JerseyColor jersey) const {
    switch (jersey) {
        case JerseyColor::TEAL:  return kinect::theme::jerseys::TEAL_GLOW;
        case JerseyColor::CORAL: return kinect::theme::jerseys::CORAL_GLOW;
        case JerseyColor::GOLD:  return kinect::theme::jerseys::GOLD_GLOW;
        default: return kinect::theme::jerseys::TEAL_GLOW;
    }
}

void Application::getBackgroundClearColor(BackgroundTheme theme, float* out) const {
    if (theme == BackgroundTheme::DAY) {
        kinect::theme::backgrounds::getDayColor(out);
    } else {
        kinect::theme::backgrounds::getNightColor(out);
    }
}
```

### 4.2 Mini Skeleton Preview

```cpp
void Application::renderMiniSkeleton(ImDrawList* drawList, ImVec2 center, float scale, ImU32 color) {
    // Simplified skeleton for preview
    float s = scale;

    // Body
    ImVec2 head(center.x, center.y - 60 * s);
    ImVec2 neck(center.x, center.y - 45 * s);
    ImVec2 chest(center.x, center.y - 20 * s);
    ImVec2 pelvis(center.x, center.y + 10 * s);

    // Arms
    ImVec2 lShoulder(center.x - 25 * s, center.y - 35 * s);
    ImVec2 rShoulder(center.x + 25 * s, center.y - 35 * s);
    ImVec2 lHand(center.x - 40 * s, center.y);
    ImVec2 rHand(center.x + 40 * s, center.y);

    // Legs
    ImVec2 lFoot(center.x - 20 * s, center.y + 70 * s);
    ImVec2 rFoot(center.x + 20 * s, center.y + 70 * s);

    // Draw bones with jersey color
    float thickness = 3.0f * s;
    drawList->AddLine(head, neck, color, thickness);
    drawList->AddLine(neck, chest, color, thickness);
    drawList->AddLine(chest, pelvis, color, thickness);
    drawList->AddLine(chest, lShoulder, color, thickness);
    drawList->AddLine(chest, rShoulder, color, thickness);
    drawList->AddLine(lShoulder, lHand, color, thickness);
    drawList->AddLine(rShoulder, rHand, color, thickness);
    drawList->AddLine(pelvis, lFoot, color, thickness);
    drawList->AddLine(pelvis, rFoot, color, thickness);

    // Draw joints
    float jointRadius = 4.0f * s;
    drawList->AddCircleFilled(head, jointRadius * 1.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircleFilled(lHand, jointRadius, color);
    drawList->AddCircleFilled(rHand, jointRadius, color);
    drawList->AddCircleFilled(lFoot, jointRadius, color);
    drawList->AddCircleFilled(rFoot, jointRadius, color);
}
```

### 4.3 Options Screen Rendering

```cpp
void Application::renderOptionsSelect() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
    ImGui::Begin("Options", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float centerX = width_ / 2.0f;
    float time = static_cast<float>(ImGui::GetTime());

    // Title
    const char* title = "CUSTOMIZE YOUR PLAYER";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(nullptr, 48.0f, ImVec2(centerX - titleSize.x * 1.2f, 120),
        kinect::theme::colors::WHITE, title);

    // ========== JERSEY SELECTION ==========
    const char* jerseyLabel = "SELECT JERSEY COLOR";
    ImVec2 jerseyLabelSize = ImGui::CalcTextSize(jerseyLabel);
    drawList->AddText(ImVec2(centerX - jerseyLabelSize.x / 2, 280),
        kinect::theme::colors::GRAY, jerseyLabel);

    // Jersey buttons
    float buttonW = 280.0f;
    float buttonH = 120.0f;
    float buttonSpacing = 30.0f;
    float totalButtonsW = buttonW * 3 + buttonSpacing * 2;
    float buttonStartX = centerX - totalButtonsW / 2;
    float buttonY = 360.0f;

    struct JerseyOption {
        const char* name;
        JerseyColor color;
        ImU32 displayColor;
    };

    JerseyOption jerseys[] = {
        {"TEAL", JerseyColor::TEAL, kinect::theme::jerseys::TEAL},
        {"CORAL", JerseyColor::CORAL, kinect::theme::jerseys::CORAL},
        {"GOLD", JerseyColor::GOLD, kinect::theme::jerseys::GOLD}
    };

    for (int i = 0; i < 3; i++) {
        float bx = buttonStartX + i * (buttonW + buttonSpacing);
        ImVec2 bMin(bx, buttonY);
        ImVec2 bMax(bx + buttonW, buttonY + buttonH);

        bool isSelected = (selectedJersey_ == jerseys[i].color);
        bool isHovered = ImGui::IsMouseHoveringRect(bMin, bMax);

        // Button background
        ImU32 bgColor = isSelected ? jerseys[i].displayColor : IM_COL32(30, 40, 60, 200);
        drawList->AddRectFilled(bMin, bMax, bgColor, 12.0f);

        // Border (highlight if selected or hovered)
        ImU32 borderColor = isSelected ? kinect::theme::colors::WHITE :
                           (isHovered ? jerseys[i].displayColor : kinect::theme::colors::BORDER_SUBTLE);
        drawList->AddRect(bMin, bMax, borderColor, 12.0f, 0, isSelected ? 4.0f : 2.0f);

        // Jersey color preview square
        float previewSize = 40.0f;
        ImVec2 previewMin(bx + 20, buttonY + (buttonH - previewSize) / 2);
        ImVec2 previewMax(previewMin.x + previewSize, previewMin.y + previewSize);
        drawList->AddRectFilled(previewMin, previewMax, jerseys[i].displayColor, 6.0f);

        // Label
        ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : kinect::theme::colors::WHITE;
        drawList->AddText(ImVec2(bx + 80, buttonY + (buttonH - 20) / 2), textColor, jerseys[i].name);

        // Handle click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            selectedJersey_ = jerseys[i].color;
        }
    }

    // ========== SKELETON PREVIEW ==========
    float previewY = 650.0f;
    ImU32 jerseyColor = getJerseyColor(selectedJersey_);

    // Preview background (shows selected background theme)
    float previewBgW = 400.0f;
    float previewBgH = 300.0f;
    ImVec2 previewBgMin(centerX - previewBgW / 2, previewY - 20);
    ImVec2 previewBgMax(centerX + previewBgW / 2, previewY + previewBgH);

    ImU32 previewBgColor = (selectedBackground_ == BackgroundTheme::DAY) ?
        IM_COL32(135, 206, 235, 200) : IM_COL32(10, 22, 40, 200);
    drawList->AddRectFilled(previewBgMin, previewBgMax, previewBgColor, 12.0f);
    drawList->AddRect(previewBgMin, previewBgMax, kinect::theme::colors::BORDER, 12.0f);

    // Render mini skeleton with selected jersey color
    renderMiniSkeleton(drawList, ImVec2(centerX, previewY + 140), 1.8f, jerseyColor);

    // Preview label
    const char* previewLabel = "PREVIEW";
    ImVec2 previewLabelSize = ImGui::CalcTextSize(previewLabel);
    drawList->AddText(ImVec2(centerX - previewLabelSize.x / 2, previewY + previewBgH + 10),
        kinect::theme::colors::GRAY, previewLabel);

    // ========== BACKGROUND SELECTION ==========
    float bgSectionY = 1050.0f;
    const char* bgLabel = "SELECT TIME OF DAY";
    ImVec2 bgLabelSize = ImGui::CalcTextSize(bgLabel);
    drawList->AddText(ImVec2(centerX - bgLabelSize.x / 2, bgSectionY),
        kinect::theme::colors::GRAY, bgLabel);

    // Background toggle buttons
    float bgButtonW = 380.0f;
    float bgButtonH = 120.0f;
    float bgSpacing = 40.0f;
    float bgStartX = centerX - (bgButtonW * 2 + bgSpacing) / 2;
    float bgButtonY = bgSectionY + 80;

    struct BgOption {
        const char* name;
        const char* icon;
        BackgroundTheme theme;
        ImU32 previewColor;
    };

    BgOption backgrounds[] = {
        {"DAY", "  ", BackgroundTheme::DAY, IM_COL32(135, 206, 235, 255)},
        {"NIGHT", "  ", BackgroundTheme::NIGHT, IM_COL32(10, 22, 40, 255)}
    };

    for (int i = 0; i < 2; i++) {
        float bx = bgStartX + i * (bgButtonW + bgSpacing);
        ImVec2 bMin(bx, bgButtonY);
        ImVec2 bMax(bx + bgButtonW, bgButtonY + bgButtonH);

        bool isSelected = (selectedBackground_ == backgrounds[i].theme);
        bool isHovered = ImGui::IsMouseHoveringRect(bMin, bMax);

        // Button background with theme preview
        drawList->AddRectFilled(bMin, bMax, backgrounds[i].previewColor, 12.0f);

        // Overlay for non-selected
        if (!isSelected) {
            drawList->AddRectFilled(bMin, bMax, IM_COL32(0, 0, 0, 100), 12.0f);
        }

        // Border
        ImU32 borderColor = isSelected ? kinect::theme::colors::TEAL :
                           (isHovered ? kinect::theme::colors::TEAL : kinect::theme::colors::BORDER_SUBTLE);
        drawList->AddRect(bMin, bMax, borderColor, 12.0f, 0, isSelected ? 4.0f : 2.0f);

        // Label
        ImU32 textColor = kinect::theme::colors::WHITE;
        float textX = bx + (bgButtonW - 60) / 2;
        drawList->AddText(nullptr, 32.0f, ImVec2(textX, bgButtonY + (bgButtonH - 32) / 2), textColor, backgrounds[i].name);

        // Handle click
        if (isHovered && ImGui::IsMouseClicked(0)) {
            selectedBackground_ = backgrounds[i].theme;
        }
    }

    // ========== CONTINUE BUTTON ==========
    float continueY = 1700.0f;
    float continueW = 500.0f;
    float continueH = 100.0f;
    ImVec2 continueMin(centerX - continueW / 2, continueY);
    ImVec2 continueMax(centerX + continueW / 2, continueY + continueH);

    bool continueHovered = ImGui::IsMouseHoveringRect(continueMin, continueMax);

    // Pulsing effect
    float pulse = 0.8f + 0.2f * sinf(time * 3.0f);
    ImU32 continueBg = continueHovered ?
        kinect::theme::colors::TEAL_HOVER :
        IM_COL32(0, static_cast<int>(212 * pulse), static_cast<int>(170 * pulse), 255);

    drawList->AddRectFilled(continueMin, continueMax, continueBg, 16.0f);
    drawList->AddRect(continueMin, continueMax, kinect::theme::colors::WHITE, 16.0f, 0, 2.0f);

    const char* continueText = "CONTINUE";
    ImVec2 continueTextSize = ImGui::CalcTextSize(continueText);
    drawList->AddText(nullptr, 40.0f,
        ImVec2(centerX - continueTextSize.x * 1.1f, continueY + (continueH - 40) / 2),
        IM_COL32(0, 0, 0, 255), continueText);

    // Handle continue click
    if (continueHovered && ImGui::IsMouseClicked(0)) {
        transitionTo(GameState::SelectingChallenge);
    }

    // ========== TIMEOUT INDICATOR ==========
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - stateStartTime_).count();
    float remaining = 30.0f - elapsed;

    if (remaining < 10.0f && remaining > 0) {
        char timeoutStr[32];
        snprintf(timeoutStr, sizeof(timeoutStr), "Auto-continue in %d...", static_cast<int>(remaining) + 1);
        ImVec2 timeoutSize = ImGui::CalcTextSize(timeoutStr);
        drawList->AddText(ImVec2(centerX - timeoutSize.x / 2, 1820),
            kinect::theme::colors::CORAL, timeoutStr);
    }

    ImGui::End();
}
```

### 4.4 Update render() Method

Modify the background clear color:
```cpp
void Application::render() {
    // ... existing code ...

    // Clear background based on selected theme
    float clearColor[4];
    getBackgroundClearColor(selectedBackground_, clearColor);
    d3dContext_->ClearRenderTargetView(renderTarget_, clearColor);

    // ... rest of render code ...
}
```

Add case in render switch:
```cpp
case GameState::SelectingOptions:
    renderOptionsSelect();
    break;
```

### 4.5 Update State Transitions

In `updateStateLogic()`:
```cpp
case GameState::PlayerDetected: {
    auto now = std::chrono::steady_clock::now();
    float elapsedSec = std::chrono::duration<float>(now - stateStartTime_).count();
    if (elapsedSec > 2.0f) {
        transitionTo(GameState::SelectingOptions);  // Changed from SelectingChallenge
    }
    break;
}

case GameState::SelectingOptions: {
    auto now = std::chrono::steady_clock::now();
    float elapsedSec = std::chrono::duration<float>(now - stateStartTime_).count();
    // Auto-continue after 30 seconds
    if (elapsedSec > 30.0f) {
        transitionTo(GameState::SelectingChallenge);
    }
    break;
}
```

### 4.6 Update Skeleton Rendering

Modify `renderDemoSkeleton()` to use selected jersey color:
```cpp
void Application::renderDemoSkeleton() {
    // ... existing setup ...

    // Use selected jersey color instead of hardcoded
    ImU32 boneColor = getJerseyColor(selectedJersey_);
    ImU32 glowColor = getJerseyGlowColor(selectedJersey_);
    ImU32 jointColor = kinect::theme::colors::JOINT;
    ImU32 kickFootColor = kinect::theme::colors::KICK_FOOT;

    // ... rest of skeleton rendering unchanged ...
}
```

---

## 5. SessionManager Updates

### 5.1 SessionManager.h

```cpp
// Add method declaration
void setPlayerOptions(const std::string& sessionId,
                      JerseyColor jersey,
                      BackgroundTheme background);
```

### 5.2 SessionManager.cpp

```cpp
void SessionManager::setPlayerOptions(const std::string& sessionId,
                                      JerseyColor jersey,
                                      BackgroundTheme background) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second.selectedJersey = jersey;
        it->second.selectedBackground = background;
        LOG_INFO("Player options set for session " << sessionId
                 << ": jersey=" << static_cast<int>(jersey)
                 << ", background=" << static_cast<int>(background));
    }
}
```

---

## 6. Day Theme Color Adjustments

For DAY mode, adjust some UI colors for better visibility:

```cpp
// In render methods, check background theme for contrast
ImU32 getTextColorForTheme(BackgroundTheme theme) {
    return (theme == BackgroundTheme::DAY) ?
        IM_COL32(20, 40, 60, 255) :   // Dark text on light bg
        IM_COL32(255, 255, 255, 255); // White text on dark bg
}
```

---

## 7. Testing Checklist

- [ ] Jersey buttons render correctly
- [ ] Jersey selection highlights properly
- [ ] Background buttons render correctly
- [ ] Background selection updates preview
- [ ] Preview skeleton shows selected jersey color
- [ ] Preview background matches selection
- [ ] Continue button transitions to challenge select
- [ ] 30s timeout auto-continues
- [ ] Selected jersey color persists to gameplay
- [ ] Background theme persists to gameplay
- [ ] Day mode has readable text/UI
- [ ] Night mode matches original theme

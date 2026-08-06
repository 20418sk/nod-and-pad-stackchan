#pragma once

#include <M5GFX.h>

#include <cstdint>

enum class FaceExpression : uint8_t {
    NORMAL,
    LISTENING,
    NODDING,
    PETTING,
    SLEEPING,
    ERROR,
};

struct FaceDebugInfo {
    const char* stateName{"STARTUP"};
    const char* directionName{"?"};
    float smoothedLevel{0.0F};
    float leftLevel{0.0F};
    float rightLevel{0.0F};
    float noiseFloor{0.0F};
    float dynamicThreshold{0.0F};
    float directionConfidence{0.0F};
    int directionLagSamples{0};
    int yawTarget{0};
    uint32_t lastSpeechMs{0};
};

class FaceRenderer {
public:
    bool begin(LGFX_Device& display);
    void setDebugEnabled(bool enabled);
    bool debugEnabled() const { return debugEnabled_; }
    void setPettingTapStyle(bool enabled);
    void startNodAfterglow(uint32_t nowMs);

    void render(FaceExpression expression, const FaceDebugInfo& debugInfo,
                uint32_t nowMs, bool force = false);
    void showStartupCheck();
    void showCalibration(bool startup, uint8_t secondsRemaining);
    void showStartupGuide(uint8_t page);
    void showError(const char* shortMessage);
    void clearOverlay();

private:
    void drawFace(FaceExpression expression);
    bool updateAnimationState(FaceExpression expression, uint32_t nowMs,
                              bool reset);
    static bool supportsIdleAnimation(FaceExpression expression);
    void drawDebug(const FaceDebugInfo& info);
    void present();
    void drawRoundEye(int x, int y, int radius);
    void drawHappyEye(int x, int y, bool left);
    void drawSleepyEye(int x, int y);
    void drawSleepIndicator(uint8_t stage);
    void drawOfficialMouth(int weight);
    void drawHeart(int x, int y, int radius = 7);
    void drawSmallHeart(int x, int y);
    void drawBlush(int x, int y);
    void drawThickLine(int x0, int y0, int x1, int y1, int thickness,
                       uint16_t color);

    LGFX_Device* display_{nullptr};
    M5Canvas canvas_{};
    lgfx::LovyanGFX* drawTarget_{nullptr};
    FaceExpression lastExpression_{FaceExpression::ERROR};
    uint32_t lastDebugDrawMs_{0};
    uint32_t expressionStartedMs_{0};
    int8_t faceOffsetY_{0};
    int8_t gazeOffsetX_{0};
    bool blinkClosed_{false};
    bool listeningPulse_{false};
    uint32_t pettingFadeStartedMs_{0};
    uint8_t pettingFadeStage_{0};
    bool pettingFadeActive_{false};
    uint8_t pettingHeartPhase_{0};
    uint8_t pettingHeartRadius_{7};
    bool pettingTapStyle_{false};
    uint32_t nodAfterglowStartedMs_{0};
    uint8_t nodAfterglowStage_{0};
    bool nodAfterglowActive_{false};
    uint32_t wakeAnimationStartedMs_{0};
    uint8_t sleepTransitionStage_{0};
    uint8_t sleepIndicatorCount_{1};
    uint8_t wakeTransitionStage_{0};
    bool wakeAnimationActive_{false};
    bool debugEnabled_{false};
    bool overlayActive_{false};
    bool hasDrawnFace_{false};
    bool canvasReady_{false};
};

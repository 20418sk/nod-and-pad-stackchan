#pragma once

#include "AudioDirectionEstimator.h"

#include <cstdint>

struct SoundTrackingDecision {
    bool turnRequested{false};
    bool initialTurn{false};
    bool returningHome{false};
    int targetYaw{0};
    SoundDirection direction{SoundDirection::UNKNOWN};
};

// 約360 msの方向多数決と、横首の段階的な目標角を管理する純粋ロジック。
// サーボやArduinoへ依存しないため、逆判定・時刻周回をnativeで検証できる。
class SoundDirectionTracker {
public:
    void beginSession();
    void endSession();

    SoundTrackingDecision update(uint32_t nowMs, bool sampleAvailable,
                                 bool measurementAllowed,
                                 SoundDirection direction);
    SoundTrackingDecision requestReturnHome();

    bool sessionActive() const { return sessionActive_; }
    bool hasYawOffset() const { return targetYaw_ != 0; }
    int targetYaw() const { return targetYaw_; }

    static bool elapsed(uint32_t nowMs, uint32_t sinceMs,
                        uint32_t durationMs);

private:
    void resetVoteWindow();
    void suspendMeasurements();
    SoundTrackingDecision evaluateWindow(bool fastInitial);
    static int clampYaw(int yaw);

    uint32_t windowStartedMs_{0};
    uint16_t leftVotes_{0};
    uint16_t centerVotes_{0};
    uint16_t rightVotes_{0};
    int targetYaw_{0};
    SoundDirection lockedDirection_{SoundDirection::UNKNOWN};
    SoundDirection oppositeCandidate_{SoundDirection::UNKNOWN};
    uint8_t oppositeConfirmations_{0};
    bool sessionActive_{false};
    bool windowActive_{false};
    bool fastInitialEvaluated_{false};
};

#pragma once

#include <array>
#include <cstdint>

enum class HeadPetGestureType : uint8_t {
    NONE,
    SWIPE,
    SINGLE_TAP,
};

// StackChan-BSPが公開する頭部3領域の生タッチ強度から、
// 隣接する2領域のなで移動を検出する純粋ロジック。
// ハードウェアには依存しない。
class HeadPetGestureDetector {
public:
    HeadPetGestureDetector(uint32_t minimumMoveMs,
                           uint32_t maximumGestureMs,
                           uint32_t releaseResetMs,
                           uint32_t tapMinimumContactMs,
                           uint32_t tapMaximumContactMs);

    // 隣接する2領域の移動、または短い1回タップで1回だけtrueを返す。
    // 次のジェスチャーは、全領域から指を離した後に再び有効になる。
    bool update(uint32_t nowMs,
                const std::array<uint8_t, 3>& intensities);

    bool contactActive() const { return contactActive_; }
    HeadPetGestureType lastGestureType() const { return lastGestureType_; }

private:
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs,
                        uint32_t durationMs);
    void resetContact();

    uint32_t minimumMoveMs_;
    uint32_t maximumGestureMs_;
    uint32_t releaseResetMs_;
    uint32_t tapMinimumContactMs_;
    uint32_t tapMaximumContactMs_;
    uint32_t contactStartedMs_{0};
    uint32_t releaseStartedMs_{0};
    uint32_t tapContactStartedMs_{0};
    std::array<bool, 3> previousTouched_{{false, false, false}};
    std::array<bool, 3> seenZones_{{false, false, false}};
    bool contactActive_{false};
    bool releasePending_{false};
    bool gestureReported_{false};
    bool gestureExpired_{false};
    bool tapContactActive_{false};
    HeadPetGestureType lastGestureType_{HeadPetGestureType::NONE};
};

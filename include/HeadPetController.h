#pragma once

#include <cstdint>

struct HeadPetUpdate {
    bool entered{false};
    bool swipeAccepted{false};
    bool restored{false};
    bool visualChanged{false};
};

// Pure logic with the same role as HeadPetModifier in the official firmware.
// main.cpp supplies hardware input so native tests can verify time boundaries.
class HeadPetController {
public:
    HeadPetController(uint32_t restoreDelayMs, uint32_t decorationDurationMs)
        : restoreDelayMs_(restoreDelayMs),
          decorationDurationMs_(decorationDurationMs)
    {
    }

    HeadPetUpdate update(uint32_t nowMs, bool swiped, bool released);

    bool active() const { return active_; }
    bool decorated() const { return decorated_; }

private:
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs);

    uint32_t restoreDelayMs_;
    uint32_t decorationDurationMs_;
    uint32_t lastSwipeMs_{0};
    uint32_t releasedMs_{0};
    bool active_{false};
    bool decorated_{false};
    bool restorePending_{false};
};

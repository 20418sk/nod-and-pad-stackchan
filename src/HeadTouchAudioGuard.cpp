#include "HeadTouchAudioGuard.h"

bool HeadTouchAudioGuard::elapsed(uint32_t nowMs, uint32_t sinceMs,
                                  uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

bool HeadTouchAudioGuard::update(uint32_t nowMs, bool activity)
{
    if (activity) {
        const bool newlySuppressed = !suppressed_;
        suppressed_ = true;
        lastActivityMs_ = nowMs;
        return newlySuppressed;
    }

    if (suppressed_ && elapsed(nowMs, lastActivityMs_, quietTailMs_)) {
        suppressed_ = false;
    }
    return false;
}

#pragma once

#include <cstdint>

class HeadTouchAudioGuard {
public:
    explicit HeadTouchAudioGuard(uint32_t quietTailMs)
        : quietTailMs_(quietTailMs)
    {
    }

    // Pass contact or release as activity. Return true only when suppression starts.
    bool update(uint32_t nowMs, bool activity);
    bool suppressed() const { return suppressed_; }

private:
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs,
                        uint32_t durationMs);

    uint32_t quietTailMs_;
    uint32_t lastActivityMs_{0};
    bool suppressed_{false};
};

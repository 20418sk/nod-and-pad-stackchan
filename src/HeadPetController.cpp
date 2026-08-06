#include "HeadPetController.h"

bool HeadPetController::elapsed(uint32_t nowMs, uint32_t sinceMs,
                                uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

HeadPetUpdate HeadPetController::update(uint32_t nowMs, bool swiped,
                                        bool released)
{
    HeadPetUpdate output;

    if (active_ && decorated_ &&
        elapsed(nowMs, lastSwipeMs_, decorationDurationMs_)) {
        decorated_          = false;
        output.visualChanged = true;
    }

    if (swiped) {
        output.swipeAccepted = true;
        if (!active_) {
            active_       = true;
            output.entered = true;
        }

        restorePending_ = false;
        lastSwipeMs_    = nowMs;
        if (!decorated_) {
            decorated_          = true;
            output.visualChanged = true;
        }
    }

    if (released && active_) {
        restorePending_ = true;
        releasedMs_     = nowMs;
    }

    if (active_ && restorePending_ &&
        elapsed(nowMs, releasedMs_, restoreDelayMs_)) {
        active_              = false;
        decorated_           = false;
        restorePending_      = false;
        output.restored      = true;
        output.visualChanged = true;
    }

    return output;
}

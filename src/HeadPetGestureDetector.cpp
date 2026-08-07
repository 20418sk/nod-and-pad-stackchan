#include "HeadPetGestureDetector.h"

HeadPetGestureDetector::HeadPetGestureDetector(uint32_t minimumMoveMs,
                                               uint32_t maximumGestureMs,
                                               uint32_t releaseResetMs,
                                               uint32_t tapMinimumContactMs,
                                               uint32_t tapMaximumContactMs)
    : minimumMoveMs_(minimumMoveMs),
      maximumGestureMs_(maximumGestureMs),
      releaseResetMs_(releaseResetMs),
      tapMinimumContactMs_(tapMinimumContactMs),
      tapMaximumContactMs_(tapMaximumContactMs)
{
}

bool HeadPetGestureDetector::elapsed(uint32_t nowMs, uint32_t sinceMs,
                                     uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

void HeadPetGestureDetector::resetContact()
{
    previousTouched_.fill(false);
    seenZones_.fill(false);
    contactActive_  = false;
    releasePending_ = false;
    gestureReported_ = false;
    gestureExpired_ = false;
    tapContactActive_ = false;
}

bool HeadPetGestureDetector::update(
    uint32_t nowMs, const std::array<uint8_t, 3>& intensities)
{
    lastGestureType_ = HeadPetGestureType::NONE;
    std::array<bool, 3> touched{};
    bool anyTouched = false;
    for (std::size_t i = 0; i < touched.size(); ++i) {
        touched[i] = intensities[i] > 0;
        anyTouched = anyTouched || touched[i];
    }

    // Accept one short touch only after every head zone is fully released.
    bool singleTapped = false;
    if (anyTouched && !tapContactActive_) {
        tapContactActive_  = true;
        tapContactStartedMs_ = nowMs;
    } else if (!anyTouched && tapContactActive_) {
        tapContactActive_ = false;
        const uint32_t contactMs =
            static_cast<uint32_t>(nowMs - tapContactStartedMs_);
        singleTapped = !gestureReported_ &&
                       contactMs >= tapMinimumContactMs_ &&
                       contactMs <= tapMaximumContactMs_;
    }

    if (singleTapped) {
        resetContact();
        lastGestureType_ = HeadPetGestureType::SINGLE_TAP;
        return true;
    }

    // Treat a long no-contact gap as a separate gesture, even if one loop was delayed.
    if (anyTouched && releasePending_ &&
        elapsed(nowMs, releaseStartedMs_, releaseResetMs_)) {
        resetContact();
    }

    if (!anyTouched) {
        previousTouched_.fill(false);
        if (!contactActive_) {
            return false;
        }
        if (!releasePending_) {
            releasePending_   = true;
            releaseStartedMs_ = nowMs;
        } else if (elapsed(nowMs, releaseStartedMs_, releaseResetMs_)) {
            resetContact();
        }
        return false;
    }

    if (!contactActive_) {
        contactActive_    = true;
        contactStartedMs_ = nowMs;
        previousTouched_  = touched;
        seenZones_        = touched;
        return false;
    }

    // Keep a short zero reading at a zone boundary in the same swipe gesture.
    releasePending_ = false;

    bool newlyTouched = false;
    for (std::size_t i = 0; i < touched.size(); ++i) {
        if (touched[i] && !previousTouched_[i]) {
            seenZones_[i] = true;
            newlyTouched  = true;
        }
    }
    previousTouched_ = touched;

    if (gestureReported_ || gestureExpired_) {
        return false;
    }
    if (elapsed(nowMs, contactStartedMs_, maximumGestureMs_)) {
        gestureExpired_ = true;
        return false;
    }
    if (!newlyTouched ||
        !elapsed(nowMs, contactStartedMs_, minimumMoveMs_)) {
        return false;
    }

    const bool crossedAdjacentPair =
        (seenZones_[0] && seenZones_[1]) ||
        (seenZones_[1] && seenZones_[2]);
    if (!crossedAdjacentPair) {
        return false;
    }

    gestureReported_ = true;
    lastGestureType_ = HeadPetGestureType::SWIPE;
    return true;
}

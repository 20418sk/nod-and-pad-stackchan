#include "SoundDirectionTracker.h"

#include "AppConfig.h"

bool SoundDirectionTracker::elapsed(uint32_t nowMs, uint32_t sinceMs,
                                    uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

void SoundDirectionTracker::beginSession()
{
    if (sessionActive_) {
        return;
    }
    sessionActive_ = true;
    lockedDirection_ = SoundDirection::UNKNOWN;
    oppositeCandidate_ = SoundDirection::UNKNOWN;
    oppositeConfirmations_ = 0;
    resetVoteWindow();
}

void SoundDirectionTracker::endSession()
{
    sessionActive_ = false;
    lockedDirection_ = SoundDirection::UNKNOWN;
    oppositeCandidate_ = SoundDirection::UNKNOWN;
    oppositeConfirmations_ = 0;
    resetVoteWindow();
}

SoundTrackingDecision SoundDirectionTracker::update(
    uint32_t nowMs, bool sampleAvailable, bool measurementAllowed,
    SoundDirection direction)
{
    SoundTrackingDecision decision;
    if (!sessionActive_) {
        return decision;
    }

    if (!measurementAllowed) {
        suspendMeasurements();
        return decision;
    }
    if (!sampleAvailable) {
        // 発話中の子音や音節間の短い無声音は、投票窓を破棄せず無投票とする。
        // 360 msに達した窓は有効票だけで評価し、古い票を次の窓へ残さない。
        if (windowActive_) {
            if (targetYaw_ == app_config::motion::kHomeYaw &&
                !fastInitialEvaluated_ &&
                elapsed(nowMs, windowStartedMs_,
                        app_config::audio_direction::kFastInitialWindowMs)) {
                fastInitialEvaluated_ = true;
                decision = evaluateWindow(true);
                if (decision.turnRequested) {
                    return decision;
                }
            }
            if (elapsed(nowMs, windowStartedMs_,
                        app_config::audio_direction::kVoteWindowMs)) {
                return evaluateWindow(false);
            }
        }
        return decision;
    }

    if (!windowActive_) {
        windowActive_ = true;
        windowStartedMs_ = nowMs;
        fastInitialEvaluated_ = false;
    }

    switch (direction) {
        case SoundDirection::LEFT:
            ++leftVotes_;
            break;
        case SoundDirection::CENTER:
            ++centerVotes_;
            break;
        case SoundDirection::RIGHT:
            ++rightVotes_;
            break;
        case SoundDirection::UNKNOWN:
            break;
    }

    if (targetYaw_ == app_config::motion::kHomeYaw &&
        !fastInitialEvaluated_ &&
        elapsed(nowMs, windowStartedMs_,
                app_config::audio_direction::kFastInitialWindowMs)) {
        fastInitialEvaluated_ = true;
        decision = evaluateWindow(true);
        if (decision.turnRequested) {
            return decision;
        }
    }

    if (elapsed(nowMs, windowStartedMs_,
                app_config::audio_direction::kVoteWindowMs)) {
        return evaluateWindow(false);
    }
    return decision;
}

SoundTrackingDecision SoundDirectionTracker::requestReturnHome()
{
    SoundTrackingDecision decision;
    if (targetYaw_ == app_config::motion::kHomeYaw) {
        return decision;
    }

    targetYaw_ = app_config::motion::kHomeYaw;
    lockedDirection_ = SoundDirection::UNKNOWN;
    oppositeCandidate_ = SoundDirection::UNKNOWN;
    oppositeConfirmations_ = 0;
    resetVoteWindow();

    decision.turnRequested = true;
    decision.returningHome = true;
    decision.targetYaw = targetYaw_;
    decision.direction = SoundDirection::CENTER;
    return decision;
}

void SoundDirectionTracker::resetVoteWindow()
{
    windowActive_ = false;
    windowStartedMs_ = 0;
    leftVotes_ = 0;
    centerVotes_ = 0;
    rightVotes_ = 0;
    fastInitialEvaluated_ = false;
}

void SoundDirectionTracker::suspendMeasurements()
{
    resetVoteWindow();
    // サーボ音をまたいだ逆方向候補を連結しない。
    oppositeCandidate_ = SoundDirection::UNKNOWN;
    oppositeConfirmations_ = 0;
}

SoundTrackingDecision SoundDirectionTracker::evaluateWindow(bool fastInitial)
{
    using namespace app_config::audio_direction;

    const uint16_t validVotes = static_cast<uint16_t>(
        leftVotes_ + centerVotes_ + rightVotes_);
    SoundDirection winner = SoundDirection::LEFT;
    uint16_t winnerVotes = leftVotes_;
    uint16_t secondVotes = centerVotes_ > rightVotes_ ? centerVotes_
                                                      : rightVotes_;
    if (centerVotes_ > winnerVotes) {
        winner = SoundDirection::CENTER;
        winnerVotes = centerVotes_;
        secondVotes = leftVotes_ > rightVotes_ ? leftVotes_ : rightVotes_;
    }
    if (rightVotes_ > winnerVotes) {
        winner = SoundDirection::RIGHT;
        winnerVotes = rightVotes_;
        secondVotes = leftVotes_ > centerVotes_ ? leftVotes_ : centerVotes_;
    }

    SoundTrackingDecision decision;
    const uint16_t minimumVotes = fastInitial
                                      ? kFastInitialMinimumValidVotes
                                      : kMinimumValidVotes;
    const uint8_t minimumWinnerPercent = fastInitial
                                             ? kFastInitialMinimumWinnerPercent
                                             : kMinimumWinnerPercent;
    const uint16_t minimumLead = fastInitial
                                     ? kFastInitialMinimumVoteLead
                                     : kMinimumVoteLead;
    if (validVotes < minimumVotes ||
        static_cast<uint32_t>(winnerVotes) * 100U <
            static_cast<uint32_t>(validVotes) * minimumWinnerPercent ||
        winnerVotes < secondVotes + minimumLead) {
        if (fastInitial) {
            return decision;
        }
        resetVoteWindow();
        oppositeCandidate_ = SoundDirection::UNKNOWN;
        oppositeConfirmations_ = 0;
        return decision;
    }

    if (winner == SoundDirection::CENTER) {
        if (fastInitial) {
            return decision;
        }
        resetVoteWindow();
        oppositeCandidate_ = SoundDirection::UNKNOWN;
        oppositeConfirmations_ = 0;
        return decision;
    }

    resetVoteWindow();

    const int directionSign = winner == SoundDirection::LEFT ? 1 : -1;
    const bool firstTurn = targetYaw_ == app_config::motion::kHomeYaw;
    if (firstTurn) {
        targetYaw_ = clampYaw(directionSign *
                              app_config::motion::kInitialSoundYaw);
        lockedDirection_ = winner;
    } else if (winner == lockedDirection_) {
        oppositeCandidate_ = SoundDirection::UNKNOWN;
        oppositeConfirmations_ = 0;
        const int corrected = clampYaw(
            targetYaw_ + directionSign *
                             app_config::motion::kSoundYawCorrection);
        if (corrected == targetYaw_) {
            return decision;
        }
        targetYaw_ = corrected;
    } else {
        if (oppositeCandidate_ == winner) {
            if (oppositeConfirmations_ < UINT8_MAX) {
                ++oppositeConfirmations_;
            }
        } else {
            oppositeCandidate_ = winner;
            oppositeConfirmations_ = 1;
        }
        if (oppositeConfirmations_ < kOppositeWindowConfirmations) {
            return decision;
        }

        lockedDirection_ = winner;
        oppositeCandidate_ = SoundDirection::UNKNOWN;
        oppositeConfirmations_ = 0;
        targetYaw_ = clampYaw(
            targetYaw_ + directionSign *
                             app_config::motion::kSoundYawCorrection);
    }

    decision.turnRequested = true;
    decision.initialTurn = firstTurn;
    decision.targetYaw = targetYaw_;
    decision.direction = winner;
    return decision;
}

int SoundDirectionTracker::clampYaw(int yaw)
{
    if (yaw < app_config::motion::kWorkYawMin) {
        return app_config::motion::kWorkYawMin;
    }
    if (yaw > app_config::motion::kWorkYawMax) {
        return app_config::motion::kWorkYawMax;
    }
    return yaw;
}

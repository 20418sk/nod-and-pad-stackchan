#include "ListenerStateMachine.h"

#include "AppConfig.h"

bool ListenerStateMachine::elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

void ListenerStateMachine::begin(uint32_t nowMs)
{
    state_                = ListenerState::STARTUP;
    activeReaction_       = ReactionType::NONE;
    stateSinceMs_         = nowMs;
    candidateSinceMs_     = nowMs;
    speechStartedMs_      = nowMs;
    endCandidateSinceMs_  = nowMs;
    lastActivityMs_       = nowMs;
    lastReactionMs_       = nowMs;
    lastSpeechDurationMs_ = 0;
    hasReacted_           = false;
}

void ListenerStateMachine::resetToIdle(uint32_t nowMs)
{
    state_               = ListenerState::IDLE;
    activeReaction_      = ReactionType::NONE;
    stateSinceMs_        = nowMs;
    candidateSinceMs_    = nowMs;
    speechStartedMs_     = nowMs;
    endCandidateSinceMs_ = nowMs;
    lastActivityMs_      = nowMs;
}

void ListenerStateMachine::transitionTo(ListenerState next, uint32_t nowMs,
                                        ListenerOutput& output)
{
    if (next == state_) {
        return;
    }
    output.stateChanged = true;
    output.previousState = state_;
    state_               = next;
    stateSinceMs_        = nowMs;
    output.state         = state_;
}

ReactionType ListenerStateMachine::classifySpeech(uint32_t durationMs)
{
    using namespace app_config::listener;
    if (durationMs < kMinimumSpeechMs) {
        return ReactionType::NONE;
    }
    return ReactionType::NORMAL_NOD;
}

ListenerOutput ListenerStateMachine::update(uint32_t nowMs, const ListenerInput& input)
{
    using namespace app_config::listener;

    ListenerOutput output;
    output.previousState     = state_;
    output.state             = state_;
    output.completedSpeechMs = lastSpeechDurationMs_;

    if (!input.audioHealthy) {
        return output;
    }

    if (state_ == ListenerState::STARTUP) {
        if (input.audioReady && elapsed(nowMs, stateSinceMs_, kStartupWaitMs)) {
            lastActivityMs_ = nowMs;
            transitionTo(ListenerState::IDLE, nowMs, output);
        }
        return output;
    }

    if (state_ == ListenerState::REACTING) {
        if (input.reactionComplete) {
            activeReaction_ = ReactionType::NONE;
            transitionTo(ListenerState::COOLDOWN, nowMs, output);
        }
        return output;
    }

    if (state_ == ListenerState::COOLDOWN) {
        if (elapsed(nowMs, stateSinceMs_, kCooldownMs)) {
            lastActivityMs_ = nowMs;
            transitionTo(ListenerState::IDLE, nowMs, output);
        }
        return output;
    }

    if (state_ == ListenerState::IDLE &&
        elapsed(nowMs, lastActivityMs_, kSleepAfterSilenceMs)) {
        transitionTo(ListenerState::SLEEPING, nowMs, output);
        return output;
    }

    // Ignore samples during servo motion, touch activity, and calibration.
    // This prevents mechanical sound from changing the listening state.
    if (input.detectionSuppressed || !input.sampleAvailable) {
        return output;
    }

    switch (state_) {
        case ListenerState::IDLE:
        case ListenerState::SLEEPING:
            if (input.level >= input.startThreshold) {
                const bool wasSleeping = state_ == ListenerState::SLEEPING;
                candidateSinceMs_ = nowMs;
                lastActivityMs_   = nowMs;
                transitionTo(ListenerState::SPEECH_CANDIDATE, nowMs, output);
                output.wokeFromSleep = wasSleeping;
            }
            break;

        case ListenerState::SPEECH_CANDIDATE:
            // Require a continuous level hold before confirming speech.
            // A short impact or tap should return directly to idle.
            if (input.level < input.startThreshold) {
                transitionTo(ListenerState::IDLE, nowMs, output);
            } else {
                lastActivityMs_ = nowMs;
                if (elapsed(nowMs, candidateSinceMs_, kSpeechStartHoldMs)) {
                    speechStartedMs_ = candidateSinceMs_;
                    transitionTo(ListenerState::LISTENING, nowMs, output);
                }
            }
            break;

        case ListenerState::LISTENING:
            lastActivityMs_ = nowMs;
            if (input.level < input.endThreshold) {
                endCandidateSinceMs_ = nowMs;
                transitionTo(ListenerState::END_CANDIDATE, nowMs, output);
            }
            break;

        case ListenerState::END_CANDIDATE:
            // Hysteresis keeps short pauses inside one listening session.
            if (input.level >= input.endThreshold) {
                lastActivityMs_ = nowMs;
                transitionTo(ListenerState::LISTENING, nowMs, output);
            } else if (elapsed(nowMs, endCandidateSinceMs_, kSpeechEndHoldMs)) {
                lastSpeechDurationMs_ = static_cast<uint32_t>(endCandidateSinceMs_ - speechStartedMs_);
                output.completedSpeechMs = lastSpeechDurationMs_;
                lastActivityMs_ = nowMs;

                const ReactionType reaction =
                    classifySpeech(lastSpeechDurationMs_);
                const bool intervalReady =
                    !hasReacted_ ||
                    elapsed(nowMs, lastReactionMs_,
                            kMinimumReactionIntervalMs);

                // The minimum interval prevents repeated nods during one conversation.
                if (reaction == ReactionType::NONE || !intervalReady) {
                    transitionTo(ListenerState::COOLDOWN, nowMs, output);
                } else {
                    activeReaction_ = reaction;
                    hasReacted_     = true;
                    lastReactionMs_ = nowMs;
                    transitionTo(ListenerState::REACTING, nowMs, output);
                    output.reactionRequested = true;
                    output.reaction          = reaction;
                }
            }
            break;

        case ListenerState::STARTUP:
        case ListenerState::REACTING:
        case ListenerState::COOLDOWN:
            break;
    }

    output.state = state_;
    return output;
}

const char* ListenerStateMachine::stateName(ListenerState state)
{
    switch (state) {
        case ListenerState::STARTUP:          return "STARTUP";
        case ListenerState::IDLE:             return "IDLE";
        case ListenerState::SPEECH_CANDIDATE: return "SPEECH_CAND";
        case ListenerState::LISTENING:        return "LISTENING";
        case ListenerState::END_CANDIDATE:    return "END_CAND";
        case ListenerState::REACTING:         return "REACTING";
        case ListenerState::COOLDOWN:         return "COOLDOWN";
        case ListenerState::SLEEPING:         return "SLEEPING";
    }
    return "UNKNOWN";
}

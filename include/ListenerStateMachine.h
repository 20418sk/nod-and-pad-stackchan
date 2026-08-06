#pragma once

#include <cstdint>

enum class ListenerState : uint8_t {
    STARTUP,
    IDLE,
    SPEECH_CANDIDATE,
    LISTENING,
    END_CANDIDATE,
    REACTING,
    COOLDOWN,
    SLEEPING,
};

enum class ReactionType : uint8_t {
    NONE,
    NORMAL_NOD,
};

struct ListenerInput {
    bool sampleAvailable{false};
    bool audioReady{false};
    bool audioHealthy{true};
    bool detectionSuppressed{false};
    bool reactionComplete{true};
    float level{0.0F};
    float startThreshold{0.0F};
    float endThreshold{0.0F};
};

struct ListenerOutput {
    bool stateChanged{false};
    ListenerState previousState{ListenerState::STARTUP};
    ListenerState state{ListenerState::STARTUP};
    bool reactionRequested{false};
    ReactionType reaction{ReactionType::NONE};
    bool wokeFromSleep{false};
    uint32_t completedSpeechMs{0};
};

class ListenerStateMachine {
public:
    void begin(uint32_t nowMs);
    ListenerOutput update(uint32_t nowMs, const ListenerInput& input);
    void resetToIdle(uint32_t nowMs);

    ListenerState state() const { return state_; }
    ReactionType activeReaction() const { return activeReaction_; }
    uint32_t lastSpeechDurationMs() const { return lastSpeechDurationMs_; }

    static ReactionType classifySpeech(uint32_t durationMs);
    static const char* stateName(ListenerState state);
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs);

private:
    void transitionTo(ListenerState next, uint32_t nowMs, ListenerOutput& output);

    ListenerState state_{ListenerState::STARTUP};
    ReactionType activeReaction_{ReactionType::NONE};
    uint32_t stateSinceMs_{0};
    uint32_t candidateSinceMs_{0};
    uint32_t speechStartedMs_{0};
    uint32_t endCandidateSinceMs_{0};
    uint32_t lastActivityMs_{0};
    uint32_t lastReactionMs_{0};
    uint32_t lastSpeechDurationMs_{0};
    bool hasReacted_{false};
};

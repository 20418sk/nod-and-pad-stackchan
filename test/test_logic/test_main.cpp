#include <unity.h>

#include "AppConfig.h"
#include "AudioDirectionEstimator.h"
#include "CameraFrameAnalyzer.h"
#include "FaceCandidateEstimator.h"
#include "HeadPetController.h"
#include "HeadPetGestureDetector.h"
#include "HeadTouchAudioGuard.h"
#include "ListenerStateMachine.h"
#include "SoundDirectionTracker.h"

#include <cstdint>

namespace {

ListenerInput quietInput()
{
    ListenerInput input;
    input.audioReady       = true;
    input.audioHealthy     = true;
    input.reactionComplete = true;
    input.sampleAvailable  = true;
    input.level            = 20.0F;
    input.startThreshold   = 100.0F;
    input.endThreshold     = 60.0F;
    return input;
}

ListenerInput loudInput(bool veryLoud = false)
{
    ListenerInput input = quietInput();
    input.level          = 180.0F;
    input.veryLoud       = veryLoud;
    return input;
}

void bootToIdle(ListenerStateMachine& machine, uint32_t startedAt = 0)
{
    machine.begin(startedAt);
    ListenerInput input = quietInput();
    machine.update(startedAt + app_config::listener::kStartupWaitMs, input);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));
}

void test_speech_start_requires_continuous_hold()
{
    ListenerStateMachine machine;
    bootToIdle(machine);

    machine.update(1500, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::SPEECH_CANDIDATE),
                          static_cast<int>(machine.state()));

    machine.update(1560, quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));

    machine.update(1700, loudInput());
    machine.update(1820, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::LISTENING),
                          static_cast<int>(machine.state()));
}

void test_speech_end_requires_silence_hold_and_resumes_on_voice()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    machine.update(1500, loudInput());
    machine.update(1620, loudInput());
    machine.update(2500, quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::END_CANDIDATE),
                          static_cast<int>(machine.state()));

    machine.update(2900, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::LISTENING),
                          static_cast<int>(machine.state()));

    machine.update(3200, quietInput());
    ListenerInput waiting = quietInput();
    waiting.reactionComplete = false;
    ListenerOutput output = machine.update(3800, waiting);
    TEST_ASSERT_TRUE(output.reactionRequested);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(output.reaction));
    TEST_ASSERT_EQUAL_UINT32(1700, output.completedSpeechMs);
}

void test_short_impact_and_sub_200ms_sound_are_ignored()
{
    ListenerStateMachine machine;
    bootToIdle(machine);

    machine.update(1500, loudInput(true));
    machine.update(1540, quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));

    machine.update(1700, loudInput());
    machine.update(1820, loudInput());
    machine.update(1880, quietInput());
    ListenerOutput output = machine.update(2480, quietInput());
    TEST_ASSERT_FALSE(output.reactionRequested);
    TEST_ASSERT_EQUAL_UINT32(180, output.completedSpeechMs);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::COOLDOWN),
                          static_cast<int>(machine.state()));
}

void test_speech_duration_classification_boundaries()
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NONE),
                          static_cast<int>(ListenerStateMachine::classifySpeech(199, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(200, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(1499, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(1500, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(2999, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(3000, false)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(ListenerStateMachine::classifySpeech(200, true)));
}

void test_reacting_then_cooldown_blocks_new_detection()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    machine.update(1500, loudInput());
    machine.update(1620, loudInput());
    machine.update(2200, quietInput());

    ListenerInput motionBusy = quietInput();
    motionBusy.reactionComplete = false;
    ListenerOutput reaction = machine.update(2800, motionBusy);
    TEST_ASSERT_TRUE(reaction.reactionRequested);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::REACTING),
                          static_cast<int>(machine.state()));

    machine.update(3000, motionBusy);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::REACTING),
                          static_cast<int>(machine.state()));

    ListenerInput motionDone = loudInput();
    motionDone.reactionComplete = true;
    machine.update(3100, motionDone);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::COOLDOWN),
                          static_cast<int>(machine.state()));

    machine.update(4000, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::COOLDOWN),
                          static_cast<int>(machine.state()));
    machine.update(4300, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));
}

void test_sleep_transition_and_voice_wakeup()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    const uint32_t idleAt = app_config::listener::kStartupWaitMs;
    machine.update(idleAt + app_config::listener::kSleepAfterSilenceMs,
                   quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::SLEEPING),
                          static_cast<int>(machine.state()));

    ListenerOutput output = machine.update(idleAt +
                                           app_config::listener::kSleepAfterSilenceMs + 20,
                                           loudInput());
    TEST_ASSERT_TRUE(output.wokeFromSleep);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::SPEECH_CANDIDATE),
                          static_cast<int>(machine.state()));
}

void test_detection_suppression_ignores_servo_sound()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    ListenerInput servoNoise = loudInput(true);
    servoNoise.detectionSuppressed = true;
    machine.update(1500, servoNoise);
    machine.update(1700, servoNoise);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));
}

void test_minimum_reaction_interval_blocks_only_the_early_reaction()
{
    ListenerStateMachine machine;
    bootToIdle(machine);

    machine.update(1500, loudInput());
    machine.update(1620, loudInput());
    machine.update(1700, quietInput());
    ListenerInput motionBusy = quietInput();
    motionBusy.reactionComplete = false;
    TEST_ASSERT_TRUE(machine.update(2300, motionBusy).reactionRequested);

    machine.update(2400, quietInput());
    machine.update(3600, quietInput());
    machine.update(3700, loudInput());
    machine.update(3820, loudInput());
    machine.update(4000, quietInput());
    ListenerOutput blocked = machine.update(4600, quietInput());
    TEST_ASSERT_FALSE(blocked.reactionRequested);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::COOLDOWN),
                          static_cast<int>(machine.state()));

    machine.update(5800, quietInput());
    machine.update(5900, loudInput());
    machine.update(6020, loudInput());
    machine.update(6300, quietInput());
    ListenerOutput allowed = machine.update(6900, quietInput());
    TEST_ASSERT_TRUE(allowed.reactionRequested);
}

void test_millis_overflow_safe_elapsed_and_transitions()
{
    constexpr uint32_t start = 0xFFFFFF00UL;
    TEST_ASSERT_FALSE(ListenerStateMachine::elapsed(start + 20U, start, 30U));
    TEST_ASSERT_TRUE(ListenerStateMachine::elapsed(start + 40U, start, 30U));

    ListenerStateMachine machine;
    bootToIdle(machine, start);
    const uint32_t afterBoot = start + app_config::listener::kStartupWaitMs;
    machine.update(afterBoot + 20U, loudInput());
    machine.update(afterBoot + 140U, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::LISTENING),
                          static_cast<int>(machine.state()));
}

void makeDirectionSamples(int16_t* stereo, std::size_t frameCount,
                          int rightDelaySamples)
{
    int16_t source[160]{};
    uint32_t value = 0x13579BDFU;
    for (std::size_t i = 0; i < frameCount; ++i) {
        value = value * 1664525U + 1013904223U;
        source[i] = static_cast<int16_t>(
            static_cast<int32_t>((value >> 17U) & 0x3FFFU) - 8192);
    }

    for (std::size_t i = 0; i < frameCount; ++i) {
        int leftIndex = static_cast<int>(i);
        int rightIndex = static_cast<int>(i) - rightDelaySamples;
        if (rightDelaySamples < 0) {
            leftIndex = static_cast<int>(i) + rightDelaySamples;
            rightIndex = static_cast<int>(i);
        }
        stereo[i * 2U] = leftIndex >= 0
                             ? source[static_cast<std::size_t>(leftIndex)]
                             : 0;
        stereo[i * 2U + 1U] = rightIndex >= 0
                                  ? source[static_cast<std::size_t>(rightIndex)]
                                  : 0;
    }
}

void test_audio_direction_estimates_left_center_and_right()
{
    constexpr std::size_t frames = 128;
    int16_t stereo[frames * 2U]{};

    AudioDirectionEstimator leftEstimator;
    makeDirectionSamples(stereo, frames, 2);
    AudioDirectionEstimate left = leftEstimator.update(stereo, frames);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::LEFT),
                          static_cast<int>(left.direction));
    TEST_ASSERT_EQUAL_INT(2, left.lagSamples);

    AudioDirectionEstimator rightEstimator;
    makeDirectionSamples(stereo, frames, -2);
    AudioDirectionEstimate right = rightEstimator.update(stereo, frames);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::RIGHT),
                          static_cast<int>(right.direction));
    TEST_ASSERT_EQUAL_INT(-2, right.lagSamples);

    AudioDirectionEstimator centerEstimator;
    makeDirectionSamples(stereo, frames, 0);
    AudioDirectionEstimate center = centerEstimator.update(stereo, frames);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::CENTER),
                          static_cast<int>(center.direction));
    TEST_ASSERT_EQUAL_INT(0, center.lagSamples);
}

void test_audio_direction_rejects_silence_and_short_buffer()
{
    int16_t silence[64]{};
    AudioDirectionEstimator estimator;
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SoundDirection::UNKNOWN),
        static_cast<int>(estimator.update(silence, 32).direction));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SoundDirection::UNKNOWN),
        static_cast<int>(estimator.update(silence, 4).direction));
}

void test_camera_frame_analyzer_reports_brightness_and_contrast()
{
    // RGB565 little endian: black followed by white.
    const uint8_t pixels[] = {0x00, 0x00, 0xFF, 0xFF};
    const CameraFrameStats stats = analyzeRgb565(pixels, sizeof(pixels),
                                                  2, 1, 1);
    TEST_ASSERT_TRUE(stats.valid);
    TEST_ASSERT_EQUAL_UINT8(127, stats.meanLuma);
    TEST_ASSERT_EQUAL_UINT8(255, stats.contrastRange);
    TEST_ASSERT_EQUAL_UINT32(2, stats.sampledPixels);
}

void test_camera_frame_analyzer_rejects_invalid_buffer()
{
    const uint8_t shortBuffer[] = {0x00, 0x00};
    TEST_ASSERT_FALSE(analyzeRgb565(nullptr, 0, 1, 1).valid);
    TEST_ASSERT_FALSE(analyzeRgb565(shortBuffer, sizeof(shortBuffer),
                                    2, 1).valid);
    TEST_ASSERT_FALSE(analyzeRgb565(shortBuffer, sizeof(shortBuffer),
                                    1, 1, 0).valid);
}

void test_camera_frame_analyzer_reads_grayscale_luma_directly()
{
    const uint8_t pixels[] = {0, 64, 128, 255};
    const CameraFrameStats stats = analyzeGrayscale(
        pixels, sizeof(pixels), 2, 2, 1);
    TEST_ASSERT_TRUE(stats.valid);
    TEST_ASSERT_EQUAL_UINT8(111, stats.meanLuma);
    TEST_ASSERT_EQUAL_UINT8(255, stats.contrastRange);
    TEST_ASSERT_EQUAL_UINT32(4, stats.sampledPixels);
}

void test_face_candidate_estimator_rejects_flat_image()
{
    static uint8_t image[160 * 120];
    for (uint8_t& pixel : image) {
        pixel = 100;
    }
    const FaceCandidateEstimate estimate = estimateFaceCandidate(
        image, sizeof(image), 160, 120);
    TEST_ASSERT_TRUE(estimate.valid);
    TEST_ASSERT_FALSE(estimate.detected);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FaceCandidateDirection::UNKNOWN),
        static_cast<int>(estimate.direction));
}

void test_face_candidate_estimator_reports_right_hand_pattern()
{
    static uint8_t image[160 * 120];
    for (uint8_t& pixel : image) {
        pixel = 30;
    }

    // 右側へ、左右対称で目元と口元が暗い簡易顔パターンを置く。
    constexpr std::size_t left = 80;
    constexpr std::size_t top = 17;
    constexpr std::size_t faceWidth = 60;
    constexpr std::size_t faceHeight = 75;
    for (std::size_t y = 0; y < faceHeight; ++y) {
        for (std::size_t x = 0; x < faceWidth; ++x) {
            uint8_t value = 170;
            const std::size_t yPercent = y * 100U / faceHeight;
            if (yPercent >= 25U && yPercent < 45U) {
                value = 70;
            } else if (yPercent >= 72U && yPercent < 87U) {
                value = 60;
            }
            image[(top + y) * 160U + left + x] = value;
        }
    }

    const FaceCandidateEstimate estimate = estimateFaceCandidate(
        image, sizeof(image), 160, 120);
    TEST_ASSERT_TRUE(estimate.valid);
    TEST_ASSERT_TRUE(estimate.detected);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(FaceCandidateDirection::RIGHT),
        static_cast<int>(estimate.direction));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(55, estimate.score);
}

SoundTrackingDecision feedDirectionWindow(SoundDirectionTracker& tracker,
                                          uint32_t startMs,
                                          SoundDirection direction,
                                          int wrongVoteIndex = -1)
{
    SoundTrackingDecision result;
    constexpr uint32_t samplePeriodMs = 20;
    const uint32_t sampleCount =
        (app_config::audio_direction::kVoteWindowMs / samplePeriodMs) + 1U;
    for (uint32_t i = 0; i < sampleCount; ++i) {
        SoundDirection sampleDirection = direction;
        if (static_cast<int>(i) == wrongVoteIndex) {
            sampleDirection = direction == SoundDirection::LEFT
                                  ? SoundDirection::RIGHT
                                  : SoundDirection::LEFT;
        }
        const SoundTrackingDecision decision = tracker.update(
            startMs + i * samplePeriodMs, true, true, sampleDirection);
        if (decision.turnRequested) {
            result = decision;
            break;
        }
    }
    return result;
}

void test_sound_tracking_fast_initial_turns_after_decisive_120ms()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();

    SoundTrackingDecision decision;
    for (uint32_t nowMs = 0;
         nowMs < app_config::audio_direction::kFastInitialWindowMs;
         nowMs += 20) {
        decision = tracker.update(nowMs, true, true, SoundDirection::LEFT);
        TEST_ASSERT_FALSE(decision.turnRequested);
    }
    decision = tracker.update(
        app_config::audio_direction::kFastInitialWindowMs,
        true, true, SoundDirection::LEFT);
    TEST_ASSERT_TRUE(decision.turnRequested);
    TEST_ASSERT_TRUE(decision.initialTurn);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kInitialSoundYaw,
                          decision.targetYaw);
}

void test_sound_tracking_fast_initial_rejects_ambiguous_votes()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();

    SoundTrackingDecision decision;
    for (uint32_t i = 0; i <= 6; ++i) {
        const SoundDirection direction = i < 4 ? SoundDirection::LEFT
                                               : SoundDirection::RIGHT;
        decision = tracker.update(i * 20U, true, true, direction);
    }
    TEST_ASSERT_FALSE(decision.turnRequested);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kHomeYaw, tracker.targetYaw());

    for (uint32_t i = 7; i <= 18; ++i) {
        decision = tracker.update(i * 20U, true, true,
                                  SoundDirection::LEFT);
    }
    TEST_ASSERT_TRUE(decision.turnRequested);
    TEST_ASSERT_TRUE(decision.initialTurn);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kInitialSoundYaw,
                          decision.targetYaw);
}

void test_sound_tracking_majority_ignores_single_reverse_vote()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();

    const SoundTrackingDecision decision = feedDirectionWindow(
        tracker, 100, SoundDirection::LEFT, 7);
    TEST_ASSERT_TRUE(decision.turnRequested);
    TEST_ASSERT_TRUE(decision.initialTurn);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kInitialSoundYaw,
                          decision.targetYaw);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::LEFT),
                          static_cast<int>(decision.direction));
}

void test_sound_tracking_remeasures_and_clamps_to_fifteen_degrees()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();
    TEST_ASSERT_EQUAL_INT(80, feedDirectionWindow(
                                  tracker, 0, SoundDirection::LEFT).targetYaw);

    // サーボ抑制を挟むと、次は新しい360 ms窓から測り直す。
    tracker.update(400, false, false, SoundDirection::UNKNOWN);
    SoundTrackingDecision correction = feedDirectionWindow(
        tracker, 1000, SoundDirection::LEFT);
    TEST_ASSERT_TRUE(correction.turnRequested);
    TEST_ASSERT_FALSE(correction.initialTurn);
    TEST_ASSERT_EQUAL_INT(130, correction.targetYaw);

    tracker.update(1400, false, false, SoundDirection::UNKNOWN);
    correction = feedDirectionWindow(tracker, 2000, SoundDirection::LEFT);
    TEST_ASSERT_TRUE(correction.turnRequested);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kWorkYawMax,
                          correction.targetYaw);

    tracker.update(2400, false, false, SoundDirection::UNKNOWN);
    TEST_ASSERT_FALSE(feedDirectionWindow(
                          tracker, 3000, SoundDirection::LEFT).turnRequested);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kWorkYawMax,
                          tracker.targetYaw());
}

void test_sound_tracking_requires_two_opposite_windows_before_correction()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();
    TEST_ASSERT_EQUAL_INT(80, feedDirectionWindow(
                                  tracker, 0, SoundDirection::LEFT).targetYaw);

    SoundTrackingDecision opposite = feedDirectionWindow(
        tracker, 400, SoundDirection::RIGHT);
    TEST_ASSERT_FALSE(opposite.turnRequested);
    TEST_ASSERT_EQUAL_INT(80, tracker.targetYaw());

    opposite = feedDirectionWindow(tracker, 800, SoundDirection::RIGHT);
    TEST_ASSERT_TRUE(opposite.turnRequested);
    TEST_ASSERT_EQUAL_INT(30, opposite.targetYaw);
    TEST_ASSERT_TRUE(opposite.targetYaw > 0);

    // 2窓で方向変更を確定した後だけ、次の同方向補正で中心を越えられる。
    opposite = feedDirectionWindow(tracker, 1200, SoundDirection::RIGHT);
    TEST_ASSERT_TRUE(opposite.turnRequested);
    TEST_ASSERT_EQUAL_INT(-20, opposite.targetYaw);
}

void test_sound_tracking_rejects_center_and_too_few_valid_votes()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();
    TEST_ASSERT_FALSE(feedDirectionWindow(
                          tracker, 0, SoundDirection::CENTER).turnRequested);

    SoundTrackingDecision decision;
    for (uint32_t i = 0; i <= 18; ++i) {
        const SoundDirection direction =
            i < 4 ? SoundDirection::LEFT : SoundDirection::UNKNOWN;
        decision = tracker.update(400 + i * 20U, true, true, direction);
    }
    TEST_ASSERT_FALSE(decision.turnRequested);
    TEST_ASSERT_EQUAL_INT(0, tracker.targetYaw());
}

void test_sound_tracking_suppression_discards_partial_window()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();
    for (uint32_t nowMs = 0; nowMs <= 100; nowMs += 20) {
        TEST_ASSERT_FALSE(tracker.update(nowMs, true, true,
                                         SoundDirection::LEFT).turnRequested);
    }
    tracker.update(120, false, false, SoundDirection::UNKNOWN);

    SoundTrackingDecision decision;
    for (uint32_t nowMs = 240; nowMs < 360; nowMs += 20) {
        decision = tracker.update(nowMs, true, true, SoundDirection::RIGHT);
        TEST_ASSERT_FALSE(decision.turnRequested);
    }
    decision = tracker.update(360, true, true, SoundDirection::RIGHT);
    TEST_ASSERT_TRUE(decision.turnRequested);
    TEST_ASSERT_EQUAL_INT(-app_config::motion::kInitialSoundYaw,
                          decision.targetYaw);
}

void test_sound_tracking_keeps_window_across_short_unvoiced_gaps()
{
    SoundDirectionTracker tracker;
    tracker.beginSession();

    SoundTrackingDecision decision;
    for (uint32_t i = 0; i <= 18; ++i) {
        // 自然な発話を模した短い無声音。最終フレームも無投票だが、
        // 360 ms到達時点で、それまでの有効な左票を評価できること。
        const bool voiced = i != 4 && i != 5 && i != 6 && i != 11 &&
                            i != 18;
        decision = tracker.update(i * 20U, voiced, true,
                                  voiced ? SoundDirection::LEFT
                                         : SoundDirection::UNKNOWN);
    }

    TEST_ASSERT_TRUE(decision.turnRequested);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kInitialSoundYaw,
                          decision.targetYaw);
}

void test_sound_tracking_returns_home_and_handles_millis_wrap()
{
    constexpr uint32_t start = 0xFFFFFF00UL;
    SoundDirectionTracker tracker;
    tracker.beginSession();
    const SoundTrackingDecision turn = feedDirectionWindow(
        tracker, start, SoundDirection::RIGHT);
    TEST_ASSERT_TRUE(turn.turnRequested);
    TEST_ASSERT_EQUAL_INT(-app_config::motion::kInitialSoundYaw,
                          turn.targetYaw);

    tracker.endSession();
    const SoundTrackingDecision home = tracker.requestReturnHome();
    TEST_ASSERT_TRUE(home.turnRequested);
    TEST_ASSERT_TRUE(home.returningHome);
    TEST_ASSERT_EQUAL_INT(app_config::motion::kHomeYaw, home.targetYaw);
    TEST_ASSERT_FALSE(tracker.hasYawOffset());
}

void test_head_pet_swipe_decorates_and_restores_after_release()
{
    HeadPetController pet(3000, 2000);

    HeadPetUpdate update = pet.update(100, true, false);
    TEST_ASSERT_TRUE(update.entered);
    TEST_ASSERT_TRUE(update.swipeAccepted);
    TEST_ASSERT_TRUE(pet.active());
    TEST_ASSERT_TRUE(pet.decorated());

    update = pet.update(2099, false, false);
    TEST_ASSERT_FALSE(update.visualChanged);
    TEST_ASSERT_TRUE(pet.decorated());

    update = pet.update(2100, false, false);
    TEST_ASSERT_TRUE(update.visualChanged);
    TEST_ASSERT_TRUE(pet.active());
    TEST_ASSERT_FALSE(pet.decorated());

    pet.update(2200, true, false);
    TEST_ASSERT_TRUE(pet.decorated());
    pet.update(2300, false, true);
    TEST_ASSERT_TRUE(pet.active());
    TEST_ASSERT_FALSE(pet.update(5299, false, false).restored);

    update = pet.update(5300, false, false);
    TEST_ASSERT_TRUE(update.restored);
    TEST_ASSERT_FALSE(pet.active());
    TEST_ASSERT_FALSE(pet.decorated());
}

void test_head_pet_new_swipe_cancels_pending_restore()
{
    HeadPetController pet(3000, 2000);
    pet.update(100, true, false);
    pet.update(200, false, true);
    pet.update(3000, true, false);

    TEST_ASSERT_TRUE(pet.active());
    TEST_ASSERT_FALSE(pet.update(3200, false, false).restored);

    pet.update(3300, false, true);
    TEST_ASSERT_FALSE(pet.update(6299, false, false).restored);
    TEST_ASSERT_TRUE(pet.update(6300, false, false).restored);
}

void test_head_pet_restore_is_millis_overflow_safe()
{
    constexpr uint32_t start = 0xFFFFFF00UL;
    HeadPetController pet(3000, 2000);
    pet.update(start, true, false);
    pet.update(start + 100U, false, true);

    TEST_ASSERT_FALSE(pet.update(start + 3099U, false, false).restored);
    TEST_ASSERT_TRUE(pet.update(start + 3100U, false, false).restored);
}

void test_head_pet_gesture_accepts_two_adjacent_zones()
{
    HeadPetGestureDetector gesture(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(gesture.update(100, {{1, 0, 0}}));
    TEST_ASSERT_TRUE(gesture.update(130, {{1, 2, 0}}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HeadPetGestureType::SWIPE),
                          static_cast<int>(gesture.lastGestureType()));
    TEST_ASSERT_FALSE(gesture.update(150, {{0, 2, 1}}));

    // 完全に離して再アームした後は逆方向も認識する。
    TEST_ASSERT_FALSE(gesture.update(200, {{0, 0, 0}}));
    TEST_ASSERT_FALSE(gesture.update(280, {{0, 0, 0}}));
    TEST_ASSERT_FALSE(gesture.update(300, {{0, 0, 1}}));
    TEST_ASSERT_TRUE(gesture.update(340, {{0, 1, 1}}));
}

void test_head_pet_single_tap_accepts_short_contact_and_simultaneous_touch()
{
    HeadPetGestureDetector gesture(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(gesture.update(100, {{0, 2, 0}}));
    TEST_ASSERT_TRUE(gesture.update(150, {{0, 0, 0}}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HeadPetGestureType::SINGLE_TAP),
                          static_cast<int>(gesture.lastGestureType()));

    HeadPetGestureDetector simultaneous(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(simultaneous.update(400, {{1, 1, 0}}));
    TEST_ASSERT_TRUE(simultaneous.update(450, {{0, 0, 0}}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HeadPetGestureType::SINGLE_TAP),
                          static_cast<int>(simultaneous.lastGestureType()));
}

void test_head_pet_gesture_allows_short_sensor_gap_and_wraparound()
{
    constexpr uint32_t start = 0xFFFFFFF0UL;
    HeadPetGestureDetector gesture(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(gesture.update(start, {{1, 0, 0}}));
    TEST_ASSERT_FALSE(gesture.update(start + 20U, {{0, 0, 0}}));
    TEST_ASSERT_TRUE(gesture.update(start + 60U, {{0, 1, 0}}));
}

void test_head_pet_single_tap_rejects_short_and_long_contact()
{
    HeadPetGestureDetector shortContact(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(shortContact.update(100, {{1, 0, 0}}));
    TEST_ASSERT_FALSE(shortContact.update(139, {{0, 0, 0}}));

    HeadPetGestureDetector longContact(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(longContact.update(100, {{1, 0, 0}}));
    TEST_ASSERT_FALSE(longContact.update(501, {{0, 0, 0}}));
}

void test_head_pet_single_tap_is_millis_overflow_safe()
{
    constexpr uint32_t start = 0xFFFFFFD0UL;
    HeadPetGestureDetector gesture(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(gesture.update(start, {{0, 1, 0}}));
    TEST_ASSERT_TRUE(gesture.update(start + 60U, {{0, 0, 0}}));
}

void test_head_touch_audio_guard_covers_tap_window_and_tail()
{
    HeadTouchAudioGuard guard(900);
    TEST_ASSERT_TRUE(guard.update(100, true));
    TEST_ASSERT_TRUE(guard.suppressed());

    TEST_ASSERT_FALSE(guard.update(700, true));
    TEST_ASSERT_FALSE(guard.update(1599, false));
    TEST_ASSERT_TRUE(guard.suppressed());
    TEST_ASSERT_FALSE(guard.update(1600, false));
    TEST_ASSERT_FALSE(guard.suppressed());
}

void test_head_touch_audio_guard_is_millis_overflow_safe()
{
    constexpr uint32_t start = 0xFFFFFF00UL;
    HeadTouchAudioGuard guard(900);
    TEST_ASSERT_TRUE(guard.update(start, true));
    TEST_ASSERT_FALSE(guard.update(start + 899U, false));
    TEST_ASSERT_TRUE(guard.suppressed());
    TEST_ASSERT_FALSE(guard.update(start + 900U, false));
    TEST_ASSERT_FALSE(guard.suppressed());
}


}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_speech_start_requires_continuous_hold);
    RUN_TEST(test_speech_end_requires_silence_hold_and_resumes_on_voice);
    RUN_TEST(test_short_impact_and_sub_200ms_sound_are_ignored);
    RUN_TEST(test_speech_duration_classification_boundaries);
    RUN_TEST(test_reacting_then_cooldown_blocks_new_detection);
    RUN_TEST(test_sleep_transition_and_voice_wakeup);
    RUN_TEST(test_detection_suppression_ignores_servo_sound);
    RUN_TEST(test_minimum_reaction_interval_blocks_only_the_early_reaction);
    RUN_TEST(test_millis_overflow_safe_elapsed_and_transitions);
    RUN_TEST(test_audio_direction_estimates_left_center_and_right);
    RUN_TEST(test_audio_direction_rejects_silence_and_short_buffer);
    RUN_TEST(test_camera_frame_analyzer_reports_brightness_and_contrast);
    RUN_TEST(test_camera_frame_analyzer_rejects_invalid_buffer);
    RUN_TEST(test_camera_frame_analyzer_reads_grayscale_luma_directly);
    RUN_TEST(test_face_candidate_estimator_rejects_flat_image);
    RUN_TEST(test_face_candidate_estimator_reports_right_hand_pattern);
    RUN_TEST(test_sound_tracking_fast_initial_turns_after_decisive_120ms);
    RUN_TEST(test_sound_tracking_fast_initial_rejects_ambiguous_votes);
    RUN_TEST(test_sound_tracking_majority_ignores_single_reverse_vote);
    RUN_TEST(test_sound_tracking_remeasures_and_clamps_to_fifteen_degrees);
    RUN_TEST(test_sound_tracking_requires_two_opposite_windows_before_correction);
    RUN_TEST(test_sound_tracking_rejects_center_and_too_few_valid_votes);
    RUN_TEST(test_sound_tracking_suppression_discards_partial_window);
    RUN_TEST(test_sound_tracking_keeps_window_across_short_unvoiced_gaps);
    RUN_TEST(test_sound_tracking_returns_home_and_handles_millis_wrap);
    RUN_TEST(test_head_pet_swipe_decorates_and_restores_after_release);
    RUN_TEST(test_head_pet_new_swipe_cancels_pending_restore);
    RUN_TEST(test_head_pet_restore_is_millis_overflow_safe);
    RUN_TEST(test_head_pet_gesture_accepts_two_adjacent_zones);
    RUN_TEST(test_head_pet_single_tap_accepts_short_contact_and_simultaneous_touch);
    RUN_TEST(test_head_pet_gesture_allows_short_sensor_gap_and_wraparound);
    RUN_TEST(test_head_pet_single_tap_rejects_short_and_long_contact);
    RUN_TEST(test_head_pet_single_tap_is_millis_overflow_safe);
    RUN_TEST(test_head_touch_audio_guard_covers_tap_window_and_tail);
    RUN_TEST(test_head_touch_audio_guard_is_millis_overflow_safe);
    return UNITY_END();
}

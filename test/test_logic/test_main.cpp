#include <unity.h>

#include "AppConfig.h"
#include "AudioDirectionEstimator.h"
#include "EndNodPlanner.h"
#include "HeadPetController.h"
#include "HeadPetGestureDetector.h"
#include "HeadTouchAudioGuard.h"
#include "ListenerStateMachine.h"
#include "ScreenTouchMapper.h"

#include <cstddef>
#include <cstdint>

namespace {

ListenerInput quietInput()
{
    ListenerInput input;
    input.audioReady = true;
    input.audioHealthy = true;
    input.reactionComplete = true;
    input.sampleAvailable = true;
    input.level = 20.0F;
    input.startThreshold = 100.0F;
    input.endThreshold = 60.0F;
    return input;
}

ListenerInput loudInput()
{
    ListenerInput input = quietInput();
    input.level = 180.0F;
    return input;
}

void bootToIdle(ListenerStateMachine& machine, uint32_t startedAt = 0)
{
    machine.begin(startedAt);
    machine.update(startedAt + app_config::listener::kStartupWaitMs,
                   quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));
}

void test_speech_start_and_end_require_holds()
{
    ListenerStateMachine machine;
    bootToIdle(machine);

    machine.update(1500, loudInput());
    machine.update(1560, quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));

    machine.update(1700, loudInput());
    machine.update(1820, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::LISTENING),
                          static_cast<int>(machine.state()));

    machine.update(2500, quietInput());
    machine.update(2900, loudInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::LISTENING),
                          static_cast<int>(machine.state()));

    machine.update(3200, quietInput());
    ListenerInput waiting = quietInput();
    waiting.reactionComplete = false;
    const ListenerOutput output = machine.update(3800, waiting);
    TEST_ASSERT_TRUE(output.reactionRequested);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
                          static_cast<int>(output.reaction));
    TEST_ASSERT_EQUAL_UINT32(1500, output.completedSpeechMs);
}

void test_short_sound_is_ignored()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    machine.update(1500, loudInput());
    machine.update(1620, loudInput());
    machine.update(1680, quietInput());
    const ListenerOutput output = machine.update(2280, quietInput());
    TEST_ASSERT_FALSE(output.reactionRequested);
    TEST_ASSERT_EQUAL_UINT32(180, output.completedSpeechMs);
}

void test_speech_classification_boundary()
{
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NONE),
        static_cast<int>(ListenerStateMachine::classifySpeech(199)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ReactionType::NORMAL_NOD),
        static_cast<int>(ListenerStateMachine::classifySpeech(200)));
}

void test_detection_suppression_ignores_servo_sound()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    ListenerInput servoNoise = loudInput();
    servoNoise.detectionSuppressed = true;
    machine.update(1500, servoNoise);
    machine.update(1700, servoNoise);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::IDLE),
                          static_cast<int>(machine.state()));
}

void test_sleep_and_wakeup()
{
    ListenerStateMachine machine;
    bootToIdle(machine);
    const uint32_t idleAt = app_config::listener::kStartupWaitMs;
    machine.update(idleAt + app_config::listener::kSleepAfterSilenceMs,
                   quietInput());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ListenerState::SLEEPING),
                          static_cast<int>(machine.state()));
    const ListenerOutput output = machine.update(
        idleAt + app_config::listener::kSleepAfterSilenceMs + 20,
        loudInput());
    TEST_ASSERT_TRUE(output.wokeFromSleep);
}

void test_millis_overflow_safe_elapsed()
{
    constexpr uint32_t start = 0xFFFFFF00UL;
    TEST_ASSERT_FALSE(ListenerStateMachine::elapsed(start + 20U, start, 30U));
    TEST_ASSERT_TRUE(ListenerStateMachine::elapsed(start + 40U, start, 30U));
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
                             ? source[static_cast<std::size_t>(leftIndex)] : 0;
        stereo[i * 2U + 1U] = rightIndex >= 0
                                  ? source[static_cast<std::size_t>(rightIndex)] : 0;
    }
}

void test_audio_direction_estimates_left_center_and_right()
{
    constexpr std::size_t frames = 128;
    int16_t stereo[frames * 2U]{};

    AudioDirectionEstimator leftEstimator;
    makeDirectionSamples(stereo, frames, 2);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::LEFT),
        static_cast<int>(leftEstimator.update(stereo, frames).direction));

    AudioDirectionEstimator rightEstimator;
    makeDirectionSamples(stereo, frames, -2);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::RIGHT),
        static_cast<int>(rightEstimator.update(stereo, frames).direction));

    AudioDirectionEstimator centerEstimator;
    makeDirectionSamples(stereo, frames, 0);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::CENTER),
        static_cast<int>(centerEstimator.update(stereo, frames).direction));
}

void test_audio_direction_rejects_silence()
{
    int16_t silence[64]{};
    AudioDirectionEstimator estimator;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SoundDirection::UNKNOWN),
        static_cast<int>(estimator.update(silence, 32).direction));
}

uint32_t nextRandom(uint32_t& state)
{
    state = state * 1664525U + 1013904223U;
    return state;
}

EndNodRandomValues randomValues(uint32_t& state)
{
    return {nextRandom(state), nextRandom(state), nextRandom(state),
            nextRandom(state), nextRandom(state), nextRandom(state),
            nextRandom(state), nextRandom(state)};
}

void test_end_nod_plans_stay_in_safe_ranges()
{
    EndNodPlanner planner;
    uint32_t state = 0x2468ACE1U;
    for (int i = 0; i < 10000; ++i) {
        const EndNodPlan plan = planner.next(randomValues(state));
        TEST_ASSERT_TRUE(EndNodPlanner::isSafe(plan));
        TEST_ASSERT_GREATER_OR_EQUAL_INT(app_config::motion::kWorkPitchMin,
                                         plan.targetPitch);
    }
}

void test_deep_end_nod_is_always_single()
{
    EndNodPlanner planner;
    EndNodRandomValues values{};
    values.depth = 99;
    values.count = 0;
    const EndNodPlan plan = planner.next(values);
    TEST_ASSERT_LESS_OR_EQUAL_INT(70, plan.targetPitch);
    TEST_ASSERT_EQUAL_UINT8(1, plan.count);
}

void test_shallow_end_nod_can_be_two_distinct_nods()
{
    EndNodPlanner planner;
    EndNodRandomValues values{};
    values.depth = 0;
    values.count = 0;
    const EndNodPlan plan = planner.next(values);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(90, plan.targetPitch);
    TEST_ASSERT_EQUAL_UINT8(2, plan.count);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(420, plan.betweenHoldMs);
}

void test_identical_random_input_does_not_repeat_exact_plan()
{
    EndNodPlanner planner;
    const EndNodRandomValues values{};
    const EndNodPlan first = planner.next(values);
    const EndNodPlan second = planner.next(values);
    TEST_ASSERT_NOT_EQUAL(first.downSpeed, second.downSpeed);
    TEST_ASSERT_TRUE(EndNodPlanner::isSafe(second));
}

void test_head_pet_restores_after_release_delay()
{
    HeadPetController pet(3000, 2000);
    TEST_ASSERT_TRUE(pet.update(100, true, false).entered);
    pet.update(200, false, true);
    TEST_ASSERT_FALSE(pet.update(3199, false, false).restored);
    TEST_ASSERT_TRUE(pet.update(3200, false, false).restored);
}

void test_head_pet_accepts_swipe_and_single_tap()
{
    HeadPetGestureDetector swipe(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(swipe.update(100, {{1, 0, 0}}));
    TEST_ASSERT_TRUE(swipe.update(130, {{1, 2, 0}}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HeadPetGestureType::SWIPE),
                          static_cast<int>(swipe.lastGestureType()));

    HeadPetGestureDetector tap(15, 1200, 80, 40, 400);
    TEST_ASSERT_FALSE(tap.update(100, {{0, 2, 0}}));
    TEST_ASSERT_TRUE(tap.update(150, {{0, 0, 0}}));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(HeadPetGestureType::SINGLE_TAP),
                          static_cast<int>(tap.lastGestureType()));
}

void test_head_touch_audio_guard_covers_contact_tail()
{
    HeadTouchAudioGuard guard(900);
    TEST_ASSERT_TRUE(guard.update(100, true));
    TEST_ASSERT_FALSE(guard.update(999, false));
    TEST_ASSERT_TRUE(guard.suppressed());
    TEST_ASSERT_FALSE(guard.update(1000, false));
    TEST_ASSERT_FALSE(guard.suppressed());
}

void test_screen_touch_regions_and_long_press_areas()
{
    TEST_ASSERT_EQUAL_INT(70, app_config::motion::kScreenTouchYawStep);
    TEST_ASSERT_EQUAL_INT(450, app_config::motion::kScreenTouchYawMax);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(
        app_config::motion::kScreenTouchYawMax,
        app_config::motion::kWorkYawMax);
    TEST_ASSERT_LESS_OR_EQUAL_INT(
        app_config::motion::kOfficialYawMax,
        app_config::motion::kWorkYawMax);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScreenTouchRegion::LEFT),
                          static_cast<int>(
                              ScreenTouchMapper::horizontalRegion(105, 320)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScreenTouchRegion::CENTER),
                          static_cast<int>(
                              ScreenTouchMapper::horizontalRegion(106, 320)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScreenTouchRegion::CENTER),
                          static_cast<int>(
                              ScreenTouchMapper::horizontalRegion(212, 320)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(ScreenTouchRegion::RIGHT),
                          static_cast<int>(
                              ScreenTouchMapper::horizontalRegion(213, 320)));

    TEST_ASSERT_TRUE(ScreenTouchMapper::isDebugCorner(300, 20, 320, 240));
    TEST_ASSERT_FALSE(ScreenTouchMapper::isDebugCorner(250, 20, 320, 240));
    TEST_ASSERT_FALSE(ScreenTouchMapper::isDebugCorner(300, 70, 320, 240));

    TEST_ASSERT_TRUE(
        ScreenTouchMapper::isCalibrationArea(160, 120, 320, 240));
    TEST_ASSERT_FALSE(
        ScreenTouchMapper::isCalibrationArea(20, 120, 320, 240));
    TEST_ASSERT_FALSE(
        ScreenTouchMapper::isCalibrationArea(160, 20, 320, 240));

    TEST_ASSERT_EQUAL_INT(
        70, ScreenTouchMapper::steppedYawTarget(
                0, ScreenTouchRegion::LEFT, 70, 450));
    TEST_ASSERT_EQUAL_INT(
        140, ScreenTouchMapper::steppedYawTarget(
                 70, ScreenTouchRegion::LEFT, 70, 450));
    TEST_ASSERT_EQUAL_INT(
        0, ScreenTouchMapper::steppedYawTarget(
               70, ScreenTouchRegion::RIGHT, 70, 450));
    TEST_ASSERT_EQUAL_INT(
        450, ScreenTouchMapper::steppedYawTarget(
                 420, ScreenTouchRegion::LEFT, 70, 450));
    TEST_ASSERT_EQUAL_INT(
        -450, ScreenTouchMapper::steppedYawTarget(
                  -420, ScreenTouchRegion::RIGHT, 70, 450));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_speech_start_and_end_require_holds);
    RUN_TEST(test_short_sound_is_ignored);
    RUN_TEST(test_speech_classification_boundary);
    RUN_TEST(test_detection_suppression_ignores_servo_sound);
    RUN_TEST(test_sleep_and_wakeup);
    RUN_TEST(test_millis_overflow_safe_elapsed);
    RUN_TEST(test_audio_direction_estimates_left_center_and_right);
    RUN_TEST(test_audio_direction_rejects_silence);
    RUN_TEST(test_end_nod_plans_stay_in_safe_ranges);
    RUN_TEST(test_deep_end_nod_is_always_single);
    RUN_TEST(test_shallow_end_nod_can_be_two_distinct_nods);
    RUN_TEST(test_identical_random_input_does_not_repeat_exact_plan);
    RUN_TEST(test_head_pet_restores_after_release_delay);
    RUN_TEST(test_head_pet_accepts_swipe_and_single_tap);
    RUN_TEST(test_head_touch_audio_guard_covers_contact_tail);
    RUN_TEST(test_screen_touch_regions_and_long_press_areas);
    return UNITY_END();
}

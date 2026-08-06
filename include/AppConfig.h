#pragma once

#include <cstddef>
#include <cstdint>

// 角度はStackChan-BSPに合わせた0.1度単位（200 = 20度）。
namespace app_config {

namespace audio {
constexpr uint32_t kSampleRateHz = 16000;
constexpr std::size_t kSamplesPerBlock = 320;  // 20 ms
constexpr std::size_t kStereoChannels = 2;
constexpr std::size_t kCaptureBuffers = 3;
constexpr float kLevelSmoothingAlpha = 0.25F;

constexpr float kInitialNoiseFloor = 120.0F;
constexpr float kMinimumNoiseFloor = 30.0F;
constexpr float kMaximumNoiseFloor = 8000.0F;
constexpr float kNoiseRiseAlpha = 0.010F;
constexpr float kNoiseFallAlpha = 0.002F;

constexpr float kStartThresholdRatio = 2.8F;
constexpr float kStartThresholdMargin = 160.0F;
constexpr float kEndThresholdRatio = 1.7F;
constexpr float kEndThresholdMargin = 80.0F;

constexpr uint32_t kStartupCalibrationMs = 3000;
constexpr uint32_t kManualCalibrationMs = 3000;
constexpr std::size_t kCalibrationSamples = 128;
constexpr uint8_t kMicNoiseFilterLevel = 16;
constexpr uint8_t kMaxConsecutiveCaptureFailures = 4;
}  // namespace audio

namespace audio_direction {
// 左右方向は画面上の診断表示だけに用い、サーボには接続しない。
constexpr int kMaximumLagSamples = 3;
constexpr float kMinimumCorrelation = 0.28F;
constexpr float kMinimumPeakSeparation = 0.025F;
constexpr float kSmoothingAlpha = 0.35F;
constexpr float kMinimumDecisionConfidence = 0.10F;
constexpr float kDirectionDecisionScore = 0.30F;
constexpr bool kSwapStereoChannels = false;
}  // namespace audio_direction

namespace listener {
constexpr uint32_t kStartupWaitMs = 1400;
constexpr uint32_t kSpeechStartHoldMs = 120;
constexpr uint32_t kSpeechEndHoldMs = 600;
constexpr uint32_t kMinimumSpeechMs = 200;
constexpr uint32_t kCooldownMs = 1200;
constexpr uint32_t kMinimumReactionIntervalMs = 3000;
constexpr uint32_t kSleepAfterSilenceMs = 60000;
}  // namespace listener

namespace motion {
constexpr int kOfficialYawMin = -1280;
constexpr int kOfficialYawMax = 1280;
constexpr int kWorkYawMin = -150;
constexpr int kWorkYawMax = 150;
constexpr int kHomeYaw = 0;

constexpr int kOfficialPitchMin = 50;
constexpr int kOfficialPitchMax = 850;
constexpr int kWorkPitchMin = 50;
constexpr int kWorkPitchMax = 720;
constexpr int kHomePitch = 200;
constexpr int kSleepPitch = 80;

constexpr int kListeningNodDepth = 32;
constexpr int kHeadPetRise = 70;

constexpr int kHomeSpeed = 150;
constexpr int kListeningNodSpeed = 140;
constexpr int kHeadPetSpeed = 170;

constexpr uint32_t kServoSoundSuppressionAfterMs = 650;
constexpr uint32_t kListeningNodIntervalMs = 1600;
constexpr uint32_t kServoVerificationTimeoutMs = 4200;
constexpr uint32_t kServoVerificationPollMs = 100;
constexpr int kServoVerificationTolerance = 80;
constexpr int kYawVerificationTolerance = 80;
}  // namespace motion

namespace touch {
constexpr uint32_t kDebounceMs = 40;
constexpr uint32_t kLongPressMs = 1200;
}  // namespace touch

namespace head_pet {
constexpr uint32_t kRestoreDelayMs = 2000;
constexpr uint32_t kDecorationDurationMs = 2000;
constexpr uint32_t kGestureMinimumMoveMs = 15;
constexpr uint32_t kGestureMaximumMs = 1200;
constexpr uint32_t kGestureReleaseResetMs = 80;
constexpr uint32_t kTapMinimumContactMs = 40;
constexpr uint32_t kTapMaximumContactMs = 400;
constexpr uint32_t kContactAudioSuppressionMs = 900;
}  // namespace head_pet

namespace display {
constexpr uint32_t kDebugRefreshMs = 250;
constexpr uint32_t kBlinkPeriodMs = 5200;
constexpr uint32_t kBlinkDurationMs = 140;
constexpr uint32_t kBreathingPeriodMs = 4000;
constexpr uint32_t kGazeStepMs = 2400;
constexpr uint32_t kListeningPulseMs = 900;
constexpr uint32_t kPettingDecorationFadeMs = 1200;
constexpr uint32_t kSleepFaceTransitionMs = 900;
constexpr uint32_t kWakeFaceTransitionMs = 900;
constexpr uint32_t kPettingHeartStepMs = 300;
constexpr uint32_t kPettingTapPopStepMs = 150;
constexpr uint32_t kPettingTapPopDurationMs = 600;
constexpr uint32_t kNodAfterglowDurationMs = 750;
constexpr uint32_t kSleepIndicatorStepMs = 900;
}  // namespace display

static_assert(motion::kWorkPitchMin >= motion::kOfficialPitchMin,
              "Pitch minimum must stay in the official range");
static_assert(motion::kWorkPitchMax <= motion::kOfficialPitchMax,
              "Pitch maximum must stay in the official range");
static_assert(motion::kHomePitch >= motion::kWorkPitchMin &&
                  motion::kHomePitch <= motion::kWorkPitchMax,
              "Home pitch must stay in the working range");
static_assert(motion::kSleepPitch >= motion::kWorkPitchMin &&
                  motion::kSleepPitch <= motion::kWorkPitchMax,
              "Sleep pitch must stay in the working range");
static_assert(motion::kHomePitch + motion::kHeadPetRise <=
                  motion::kWorkPitchMax,
              "Pet motion must stay in the working range");
static_assert(motion::kWorkYawMin >= motion::kOfficialYawMin &&
                  motion::kWorkYawMax <= motion::kOfficialYawMax,
              "Yaw working range must stay in the official range");
static_assert(motion::kHomeYaw >= motion::kWorkYawMin &&
                  motion::kHomeYaw <= motion::kWorkYawMax,
              "Home yaw must stay in the working range");

}  // namespace app_config

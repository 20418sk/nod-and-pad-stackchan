#pragma once

#include <cstddef>
#include <cstdint>

// 初心者向けの主な調整項目をこのファイルに集約する。
// 角度はStackChan-BSPに合わせて「0.1度単位」で指定する（450 = 45度）。
namespace app_config {

namespace audio {
constexpr uint32_t kSampleRateHz       = 16000;
constexpr std::size_t kSamplesPerBlock = 320;  // 20 ms
constexpr std::size_t kStereoChannels  = 2;
constexpr std::size_t kCaptureBuffers  = 3;    // BSP公式マイク例と同じ先行録音方式

// 音量平滑化。大きいほど反応が速いが、物音にも敏感になる。
constexpr float kLevelSmoothingAlpha = 0.25F;

// ノイズフロアの初期値と絶対保護範囲（16 bit PCMのRMS値）。
constexpr float kInitialNoiseFloor = 120.0F;
constexpr float kMinimumNoiseFloor = 30.0F;
constexpr float kMaximumNoiseFloor = 8000.0F;

// 上昇はやや速く、下降は遅くして、長い無音で推定値が崩れにくくする。
constexpr float kNoiseRiseAlpha = 0.010F;
constexpr float kNoiseFallAlpha = 0.002F;

// 動的しきい値 = max(ノイズフロア×倍率, ノイズフロア+余裕値)。
constexpr float kStartThresholdRatio  = 2.8F;
constexpr float kStartThresholdMargin = 160.0F;
constexpr float kEndThresholdRatio    = 1.7F;
constexpr float kEndThresholdMargin   = 80.0F;

// 驚き判定。生RMSがこの動的条件を超えた発話だけを対象にする。
constexpr float kVeryLoudRatio  = 4.0F;
constexpr float kVeryLoudMargin = 1200.0F;

constexpr uint32_t kStartupCalibrationMs = 1500;
constexpr uint32_t kManualCalibrationMs  = 3000;
constexpr std::size_t kCalibrationSamples = 128;
constexpr uint8_t kMicNoiseFilterLevel    = 16;
constexpr uint8_t kMaxConsecutiveCaptureFailures = 4;
}  // namespace audio

namespace audio_direction {
// 16 kHzで±3サンプルは約±0.19 ms。CoreS3前面の近接した2マイクで
// 左右の大まかな到来時間差だけを調べ、遠方角度を断定しない。
constexpr int kMaximumLagSamples = 3;
constexpr float kMinimumCorrelation = 0.28F;
constexpr float kMinimumPeakSeparation = 0.025F;
constexpr float kSmoothingAlpha = 0.35F;
constexpr float kMinimumDecisionConfidence = 0.10F;
constexpr float kDirectionDecisionScore = 0.30F;

// 20 msごとの方向値を360 ms集め、多数決で横首の根拠を作る。
// UNKNOWNが多い窓や僅差の窓では動かさない。
constexpr uint32_t kVoteWindowMs = 360;
constexpr uint16_t kMinimumValidVotes = 8;
constexpr uint8_t kMinimumWinnerPercent = 60;
constexpr uint16_t kMinimumVoteLead = 2;
// Only the first turn may use this shorter, stricter vote window.
constexpr uint32_t kFastInitialWindowMs = 120;
constexpr uint16_t kFastInitialMinimumValidVotes = 5;
constexpr uint8_t kFastInitialMinimumWinnerPercent = 70;
constexpr uint16_t kFastInitialMinimumVoteLead = 2;
// 現在向いている側と逆の窓は、2窓連続するまで補正しない。
constexpr uint8_t kOppositeWindowConfirmations = 2;

// 実機で画面左から話したときにデバッグ表示がRになる場合だけtrueへ変更する。
// 左右が未確認のまま横サーボを動かさないため、初期段階では表示診断だけに使う。
constexpr bool kSwapStereoChannels = false;
// 音方向はデバッグにだけ使い、首へ直結しない。
constexpr bool kEnableSoundServoTracking = false;
}  // namespace audio_direction

namespace listener {
constexpr uint32_t kStartupWaitMs       = 1400;
constexpr uint32_t kSpeechStartHoldMs   = 120;
constexpr uint32_t kSpeechEndHoldMs     = 600;
constexpr uint32_t kMinimumSpeechMs     = 200;
constexpr uint32_t kCooldownMs          = 1200;
constexpr uint32_t kMinimumReactionIntervalMs = 3000;
constexpr uint32_t kSleepAfterSilenceMs = 60000;
}  // namespace listener

namespace motion {
// 公式横軸範囲は-128～128度。本作品では全用途で中心±15度だけを使う。
constexpr int kOfficialYawMin = -1280;
constexpr int kOfficialYawMax = 1280;
constexpr int kWorkYawMin     = -150;
constexpr int kWorkYawMax     = 150;
constexpr int kHomeYaw        = 0;
constexpr int kInitialSoundYaw = 80;
constexpr int kSoundYawCorrection = 50;

// 公式推奨範囲は5～85度。すべての指令はまずこの範囲で制限する。
constexpr int kOfficialPitchMin = 50;
constexpr int kOfficialPitchMax = 850;

// 深いうなずきに限り公式推奨下限の5度まで使う。これより下へは指令しない。
constexpr int kWorkPitchMin = 50;
constexpr int kWorkPitchMax = 720;
constexpr int kHomePitch     = 200;
constexpr int kSleepPitch    = 80;

constexpr int kListeningNodDepth = 32;
// 製品版公式のなでなで動作を、左右旋回なし・小さな上向き動作へ安全化。
constexpr int kHeadPetRise     = 70;

// BSPの速度範囲は0～1000。小さな値から始めて急動作を避ける。
constexpr int kHomeSpeed     = 150;
constexpr int kListeningNodSpeed = 140;
constexpr int kHeadPetSpeed  = 170;
constexpr int kSoundYawInitialSpeed = 140;
constexpr int kSoundYawCorrectionSpeed = 120;
constexpr int kSoundYawReturnSpeed = 90;

constexpr uint32_t kSoundYawInitialMoveMs = 520;
constexpr uint32_t kSoundYawCorrectionMoveMs = 460;
constexpr uint32_t kSoundYawReturnMoveMs = 1200;
constexpr uint32_t kSoundYawSuppressionAfterMs = 450;

constexpr uint32_t kServoSoundSuppressionAfterMs = 650;
constexpr uint32_t kListeningNodIntervalMs = 1600;
constexpr uint32_t kServoVerificationTimeoutMs    = 2600;
constexpr uint32_t kServoVerificationPollMs       = 100;
constexpr int kServoVerificationTolerance         = 80;
constexpr int kYawVerificationTolerance            = 80;
}  // namespace motion

namespace touch {
constexpr uint32_t kDebounceMs = 40;
constexpr uint32_t kLongPressMs = 1200;
}  // namespace touch

namespace head_pet {
// 公式HeadPetModifierを基準に、手を離してから2秒後に元の表情へ戻す。
constexpr uint32_t kRestoreDelayMs = 2000;
// 公式デコレータ寿命1.5～2.5秒の中央値。挙動を安定させるため固定する。
constexpr uint32_t kDecorationDurationMs = 2000;
// 頭部3領域のうち隣接2領域を順に通れば「なで」とする。
// 同時タッチを物音のように除外しつつ、公式の3領域判定より自然に拾う設定。
constexpr uint32_t kGestureMinimumMoveMs = 15;
constexpr uint32_t kGestureMaximumMs     = 1200;
// センサー境界の短い無接触は許容する。次のなでは完全に離してから行う。
constexpr uint32_t kGestureReleaseResetMs = 80;
// 頭部の「トントン」。軽いタップを拾いやすくするため判定幅を広めにする。
// 40～400 msの短い接触を完全に離すと成立する。
constexpr uint32_t kTapMinimumContactMs   = 40;
constexpr uint32_t kTapMaximumContactMs   = 400;
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

static_assert(audio_direction::kFastInitialWindowMs <
                  audio_direction::kVoteWindowMs,
              "Fast initial vote window must be shorter than full window");

static_assert(motion::kWorkPitchMin >= motion::kOfficialPitchMin,
              "上下軸の最小角が公式安全範囲外です");
static_assert(motion::kWorkPitchMax <= motion::kOfficialPitchMax,
              "上下軸の最大角が公式安全範囲外です");
static_assert(motion::kHomePitch >= motion::kWorkPitchMin &&
                  motion::kHomePitch <= motion::kWorkPitchMax,
              "ホーム角が作品側安全範囲外です");
static_assert(motion::kSleepPitch >= motion::kWorkPitchMin &&
                  motion::kSleepPitch <= motion::kWorkPitchMax,
              "寝姿勢が作品側安全範囲外です");
static_assert(motion::kHomePitch + motion::kHeadPetRise <=
                  motion::kWorkPitchMax,
              "なでなで動作が作品側安全範囲外です");
static_assert(motion::kWorkYawMin >= motion::kOfficialYawMin &&
                  motion::kWorkYawMax <= motion::kOfficialYawMax,
              "横軸の作品側範囲が公式範囲外です");
static_assert(motion::kHomeYaw >= motion::kWorkYawMin &&
                  motion::kHomeYaw <= motion::kWorkYawMax,
              "横軸ホームが作品側安全範囲外です");
static_assert(motion::kInitialSoundYaw > 0 &&
                  motion::kInitialSoundYaw <= motion::kWorkYawMax,
              "音追尾の初回角が作品側安全範囲外です");
static_assert(motion::kSoundYawCorrection > 0 &&
                  motion::kSoundYawCorrection <=
                      (motion::kWorkYawMax - motion::kWorkYawMin),
              "音追尾の補正角が不正です");

}  // namespace app_config

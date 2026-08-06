#include "AudioDetector.h"

#include <M5Unified.h>

#include <cmath>
#include <cstdint>

namespace {
bool timeElapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}
}  // namespace

bool AudioDetector::begin()
{
    // CoreS3ではスピーカーとマイクがI2S資源を共有するため、公式例どおり
    // MVPで使わないスピーカーを明示的に停止してからマイクを開始する。
    M5.Speaker.end();

    auto micConfig               = M5.Mic.config();
    micConfig.sample_rate        = app_config::audio::kSampleRateHz;
    micConfig.noise_filter_level = app_config::audio::kMicNoiseFilterLevel;
    // CoreS3ではinput_stereoが有効。左右を保持したまま録音し、
    // 発話判定用のモノラルRMSは本クラス内で一時的に合成して求める。
    M5.Mic.config(micConfig);

    healthy_ = M5.Mic.isEnabled() && M5.Mic.begin();
    metrics_.noiseFloor = app_config::audio::kInitialNoiseFloor;
    updateThresholds();
    return healthy_;
}

bool AudioDetector::update(uint32_t nowMs, bool allowNoiseLearning,
                           bool allowDirectionEstimation)
{
    if (!healthy_) {
        return false;
    }

    // M5Unified::Micは二重キューで非同期録音する。公式MIC例と同様に、
    // 3バッファの2ブロック先へ録音し、完了済みバッファだけを解析する。
    if (!M5.Mic.record(captureBuffers_[recordIndex_],
                       app_config::audio::kSamplesPerBlock *
                           app_config::audio::kStereoChannels,
                       app_config::audio::kSampleRateHz, true)) {
        if (consecutiveCaptureFailures_ < UINT8_MAX) {
            ++consecutiveCaptureFailures_;
        }
        if (consecutiveCaptureFailures_ >= app_config::audio::kMaxConsecutiveCaptureFailures) {
            healthy_ = false;
        }
        return false;
    }

    consecutiveCaptureFailures_ = 0;
    bool producedMetrics = false;
    if (primingBlocks_ > 0) {
        --primingBlocks_;
    } else {
        processBlock(captureBuffers_[processIndex_],
                     app_config::audio::kSamplesPerBlock,
                     nowMs, allowNoiseLearning, allowDirectionEstimation);
        producedMetrics = true;
    }

    recordIndex_  = (recordIndex_ + 1U) % app_config::audio::kCaptureBuffers;
    processIndex_ = (processIndex_ + 1U) % app_config::audio::kCaptureBuffers;
    return producedMetrics;
}

void AudioDetector::calculateStereoRms(const int16_t* samples,
                                       std::size_t frameCount, float& monoRms,
                                       float& leftRms, float& rightRms) const
{
    monoRms = 0.0F;
    leftRms = 0.0F;
    rightRms = 0.0F;
    if (samples == nullptr || frameCount == 0) {
        return;
    }

    int64_t leftSum = 0;
    int64_t rightSum = 0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        leftSum += samples[i * 2U];
        rightSum += samples[i * 2U + 1U];
    }
    const int32_t leftMean = static_cast<int32_t>(
        leftSum / static_cast<int64_t>(frameCount));
    const int32_t rightMean = static_cast<int32_t>(
        rightSum / static_cast<int64_t>(frameCount));
    const int32_t monoMean = (leftMean + rightMean) / 2;

    uint64_t leftSquareSum = 0;
    uint64_t rightSquareSum = 0;
    uint64_t monoSquareSum = 0;
    for (std::size_t i = 0; i < frameCount; ++i) {
        const int32_t left = static_cast<int32_t>(samples[i * 2U]);
        const int32_t right = static_cast<int32_t>(samples[i * 2U + 1U]);
        const int32_t centeredLeft = left - leftMean;
        const int32_t centeredRight = right - rightMean;
        const int32_t centeredMono = ((left + right) / 2) - monoMean;
        leftSquareSum += static_cast<uint64_t>(
            static_cast<int64_t>(centeredLeft) * centeredLeft);
        rightSquareSum += static_cast<uint64_t>(
            static_cast<int64_t>(centeredRight) * centeredRight);
        monoSquareSum += static_cast<uint64_t>(
            static_cast<int64_t>(centeredMono) * centeredMono);
    }

    const float divisor = static_cast<float>(frameCount);
    leftRms = std::sqrt(static_cast<float>(leftSquareSum) / divisor);
    rightRms = std::sqrt(static_cast<float>(rightSquareSum) / divisor);
    monoRms = std::sqrt(static_cast<float>(monoSquareSum) / divisor);
}

void AudioDetector::processBlock(const int16_t* samples,
                                 std::size_t frameCount,
                                 uint32_t nowMs, bool allowNoiseLearning,
                                 bool allowDirectionEstimation)
{
    calculateStereoRms(samples, frameCount, metrics_.rawRms,
                       metrics_.leftRms, metrics_.rightRms);
    if (allowDirectionEstimation) {
        metrics_.direction = directionEstimator_.update(samples, frameCount);
    } else {
        // サーボ音の相関を次の投票へ持ち越さない。PCM取得自体は止めず、
        // 完了済みバッファを通常どおり上書きして古い音も残さない。
        directionEstimator_.reset();
        metrics_.direction = {};
    }
    if (!levelInitialized_) {
        metrics_.smoothedLevel = metrics_.rawRms;
        levelInitialized_      = true;
    } else {
        const float alpha = app_config::audio::kLevelSmoothingAlpha;
        metrics_.smoothedLevel += alpha * (metrics_.rawRms - metrics_.smoothedLevel);
    }

    if (calibrating_) {
        updateCalibration(metrics_.rawRms, nowMs);
    } else if (allowNoiseLearning && metrics_.smoothedLevel < metrics_.startThreshold) {
        // 一度に大きく変化させず、発話ピークや長い無音による崩壊を防ぐ。
        const float lowerBound = metrics_.noiseFloor * 0.70F;
        const float upperBound = metrics_.noiseFloor * 1.30F;
        const float boundedSample = clampFloat(metrics_.rawRms, lowerBound, upperBound);
        const float alpha = boundedSample > metrics_.noiseFloor
                                ? app_config::audio::kNoiseRiseAlpha
                                : app_config::audio::kNoiseFallAlpha;
        metrics_.noiseFloor += alpha * (boundedSample - metrics_.noiseFloor);
        metrics_.noiseFloor = clampFloat(metrics_.noiseFloor,
                                         app_config::audio::kMinimumNoiseFloor,
                                         app_config::audio::kMaximumNoiseFloor);
    }

    updateThresholds();
}

void AudioDetector::updateThresholds()
{
    const float startByRatio = metrics_.noiseFloor * app_config::audio::kStartThresholdRatio;
    const float startByMargin = metrics_.noiseFloor + app_config::audio::kStartThresholdMargin;
    metrics_.startThreshold = startByRatio > startByMargin ? startByRatio : startByMargin;

    const float endByRatio = metrics_.noiseFloor * app_config::audio::kEndThresholdRatio;
    const float endByMargin = metrics_.noiseFloor + app_config::audio::kEndThresholdMargin;
    metrics_.endThreshold = endByRatio > endByMargin ? endByRatio : endByMargin;

}

void AudioDetector::startCalibration(uint32_t nowMs, uint32_t durationMs)
{
    calibrationStartedMs_   = nowMs;
    calibrationDurationMs_  = durationMs;
    calibrationCount_       = 0;
    calibrationWriteIndex_  = 0;
    calibrationCompleted_   = false;
    calibrating_            = true;
    directionEstimator_.reset();
    metrics_.direction = {};
}

void AudioDetector::updateCalibration(float rms, uint32_t nowMs)
{
    calibrationValues_[calibrationWriteIndex_] = rms;
    calibrationWriteIndex_ = (calibrationWriteIndex_ + 1U) %
                             app_config::audio::kCalibrationSamples;
    if (calibrationCount_ < app_config::audio::kCalibrationSamples) {
        ++calibrationCount_;
    }

    if (timeElapsed(nowMs, calibrationStartedMs_, calibrationDurationMs_)) {
        finishCalibration();
    }
}

void AudioDetector::finishCalibration()
{
    if (calibrationCount_ > 0) {
        // 平均ではなく中央値を使い、較正中の単発物音の影響を抑える。
        float sorted[app_config::audio::kCalibrationSamples]{};
        for (std::size_t i = 0; i < calibrationCount_; ++i) {
            sorted[i] = calibrationValues_[i];
        }
        for (std::size_t i = 1; i < calibrationCount_; ++i) {
            const float value = sorted[i];
            std::size_t j = i;
            while (j > 0 && sorted[j - 1] > value) {
                sorted[j] = sorted[j - 1];
                --j;
            }
            sorted[j] = value;
        }
        metrics_.noiseFloor = clampFloat(sorted[calibrationCount_ / 2U],
                                         app_config::audio::kMinimumNoiseFloor,
                                         app_config::audio::kMaximumNoiseFloor);
        metrics_.smoothedLevel = metrics_.noiseFloor;
        levelInitialized_ = true;
    }

    calibrating_          = false;
    calibrationCompleted_ = true;
    updateThresholds();
}

bool AudioDetector::takeCalibrationCompleted()
{
    const bool completed = calibrationCompleted_;
    calibrationCompleted_ = false;
    return completed;
}

float AudioDetector::clampFloat(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

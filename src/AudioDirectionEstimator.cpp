#include "AudioDirectionEstimator.h"

#include "AppConfig.h"

#include <cmath>
#include <cstdint>

namespace {
int32_t differenceAt(const int16_t* samples, std::size_t frameIndex,
                     std::size_t channel)
{
    const std::size_t current = frameIndex * 2U + channel;
    const std::size_t previous = (frameIndex - 1U) * 2U + channel;
    return static_cast<int32_t>(samples[current]) - samples[previous];
}
}  // namespace

AudioDirectionEstimate AudioDirectionEstimator::update(
    const int16_t* interleavedStereo, std::size_t frameCount)
{
    using namespace app_config::audio_direction;

    AudioDirectionEstimate estimate;
    if (interleavedStereo == nullptr ||
        frameCount <= static_cast<std::size_t>((kMaximumLagSamples * 2) + 2)) {
        smoothedDirectionScore_ *= (1.0F - kSmoothingAlpha);
        smoothedConfidence_ *= (1.0F - kSmoothingAlpha);
        return estimate;
    }

    float bestCorrelation = -1.0F;
    float secondCorrelation = -1.0F;
    int bestLag = 0;
    for (int lag = -kMaximumLagSamples; lag <= kMaximumLagSamples; ++lag) {
        const float correlation = normalizedCorrelation(interleavedStereo,
                                                        frameCount, lag);
        if (correlation > bestCorrelation) {
            secondCorrelation = bestCorrelation;
            bestCorrelation = correlation;
            bestLag = lag;
        } else if (correlation > secondCorrelation) {
            secondCorrelation = correlation;
        }
    }

    const float correlationConfidence = clampFloat(
        (bestCorrelation - kMinimumCorrelation) /
            (1.0F - kMinimumCorrelation),
        0.0F, 1.0F);
    const float separationConfidence = clampFloat(
        (bestCorrelation - secondCorrelation) / kMinimumPeakSeparation,
        0.0F, 1.0F);
    const float rawConfidence = correlationConfidence * separationConfidence;

    float rawDirectionScore = 0.0F;
    if (rawConfidence > 0.0F && bestLag != 0) {
        // corr(L[t], R[t+lag])。正のlagは左マイクへ先に届いたことを表す。
        rawDirectionScore = bestLag > 0 ? 1.0F : -1.0F;
        if (kSwapStereoChannels) {
            rawDirectionScore = -rawDirectionScore;
        }
    }

    smoothedDirectionScore_ +=
        kSmoothingAlpha * (rawDirectionScore - smoothedDirectionScore_);
    smoothedConfidence_ +=
        kSmoothingAlpha * (rawConfidence - smoothedConfidence_);

    estimate.lagSamples = kSwapStereoChannels ? -bestLag : bestLag;
    estimate.correlation = bestCorrelation;
    estimate.confidence = smoothedConfidence_;
    if (bestCorrelation < kMinimumCorrelation ||
        smoothedConfidence_ < kMinimumDecisionConfidence) {
        estimate.direction = SoundDirection::UNKNOWN;
    } else if (smoothedDirectionScore_ >= kDirectionDecisionScore) {
        estimate.direction = SoundDirection::LEFT;
    } else if (smoothedDirectionScore_ <= -kDirectionDecisionScore) {
        estimate.direction = SoundDirection::RIGHT;
    } else {
        estimate.direction = SoundDirection::CENTER;
    }
    return estimate;
}

void AudioDirectionEstimator::reset()
{
    smoothedDirectionScore_ = 0.0F;
    smoothedConfidence_ = 0.0F;
}

float AudioDirectionEstimator::normalizedCorrelation(const int16_t* samples,
                                                     std::size_t frameCount,
                                                     int lag)
{
    const std::size_t offset = static_cast<std::size_t>(lag < 0 ? -lag : lag);
    const std::size_t count = frameCount - offset - 1U;
    int64_t cross = 0;
    uint64_t leftEnergy = 0;
    uint64_t rightEnergy = 0;

    for (std::size_t i = 1; i <= count; ++i) {
        const std::size_t leftIndex = lag < 0 ? i + offset : i;
        const std::size_t rightIndex = lag > 0 ? i + offset : i;
        const int32_t left = differenceAt(samples, leftIndex, 0U);
        const int32_t right = differenceAt(samples, rightIndex, 1U);
        cross += static_cast<int64_t>(left) * right;
        leftEnergy += static_cast<uint64_t>(static_cast<int64_t>(left) * left);
        rightEnergy += static_cast<uint64_t>(static_cast<int64_t>(right) * right);
    }

    if (leftEnergy == 0U || rightEnergy == 0U) {
        return 0.0F;
    }
    const double denominator =
        std::sqrt(static_cast<double>(leftEnergy) *
                  static_cast<double>(rightEnergy));
    if (denominator <= 0.0) {
        return 0.0F;
    }
    return static_cast<float>(static_cast<double>(cross) / denominator);
}

float AudioDirectionEstimator::clampFloat(float value, float minimum,
                                          float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

const char* AudioDirectionEstimator::directionName(SoundDirection direction)
{
    switch (direction) {
        case SoundDirection::UNKNOWN: return "?";
        case SoundDirection::CENTER:  return "C";
        case SoundDirection::LEFT:    return "L";
        case SoundDirection::RIGHT:   return "R";
    }
    return "?";
}

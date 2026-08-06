#include "FaceCandidateEstimator.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::size_t kSampleStep = 4;

uint8_t clampScore(uint32_t value)
{
    return static_cast<uint8_t>(std::min<uint32_t>(value, 100U));
}

uint8_t scoreWindow(const uint8_t* data, std::size_t width,
                    std::size_t left, std::size_t top,
                    std::size_t windowWidth, std::size_t windowHeight)
{
    uint64_t symmetryDifference = 0;
    uint32_t symmetryPairs = 0;
    uint64_t eyeSum = 0;
    uint32_t eyeCount = 0;
    uint64_t cheekSum = 0;
    uint32_t cheekCount = 0;
    uint64_t mouthSum = 0;
    uint32_t mouthCount = 0;
    uint8_t minimum = UINT8_MAX;
    uint8_t maximum = 0;

    for (std::size_t localY = 0; localY < windowHeight;
         localY += kSampleStep) {
        const uint32_t yPercent = static_cast<uint32_t>(
            localY * 100U / windowHeight);
        for (std::size_t localX = 0; localX < windowWidth / 2U;
             localX += kSampleStep) {
            const uint8_t leftPixel = data[(top + localY) * width +
                                           left + localX];
            const uint8_t rightPixel = data[(top + localY) * width + left +
                windowWidth - 1U - localX];
            symmetryDifference += leftPixel > rightPixel
                ? leftPixel - rightPixel : rightPixel - leftPixel;
            ++symmetryPairs;
            minimum = std::min(minimum, std::min(leftPixel, rightPixel));
            maximum = std::max(maximum, std::max(leftPixel, rightPixel));

            if (yPercent >= 25U && yPercent < 45U) {
                eyeSum += leftPixel + rightPixel;
                eyeCount += 2U;
            } else if (yPercent >= 48U && yPercent < 68U) {
                cheekSum += leftPixel + rightPixel;
                cheekCount += 2U;
            } else if (yPercent >= 72U && yPercent < 87U) {
                mouthSum += leftPixel + rightPixel;
                mouthCount += 2U;
            }
        }
    }

    if (symmetryPairs == 0 || eyeCount == 0 || cheekCount == 0 ||
        mouthCount == 0) {
        return 0;
    }

    const uint32_t averageDifference = static_cast<uint32_t>(
        symmetryDifference / symmetryPairs);
    const uint32_t symmetryScore = 100U -
        std::min<uint32_t>(averageDifference * 2U, 100U);
    const int32_t eyeDelta = static_cast<int32_t>(cheekSum / cheekCount) -
        static_cast<int32_t>(eyeSum / eyeCount);
    const int32_t mouthDelta = static_cast<int32_t>(cheekSum / cheekCount) -
        static_cast<int32_t>(mouthSum / mouthCount);
    const uint32_t contrast = maximum - minimum;

    const uint32_t score = symmetryScore * 35U / 100U +
        std::min<uint32_t>(contrast, 100U) * 20U / 100U +
        static_cast<uint32_t>(std::clamp<int32_t>(eyeDelta, 0, 20)) * 30U / 20U +
        static_cast<uint32_t>(std::clamp<int32_t>(mouthDelta, 0, 15));
    return clampScore(score);
}

}  // namespace

FaceCandidateEstimate estimateFaceCandidate(
    const uint8_t* data, std::size_t length, std::size_t width,
    std::size_t height, uint8_t detectionThreshold)
{
    FaceCandidateEstimate estimate;
    if (data == nullptr || width < 80U || height < 80U ||
        width > SIZE_MAX / height || length < width * height) {
        return estimate;
    }
    estimate.valid = true;

    const std::size_t centerY = height * 45U / 100U;
    const std::size_t scanStep = std::max<std::size_t>(8U, width / 20U);
    const std::size_t candidateWidths[] = {width / 4U, width * 3U / 8U};

    for (const std::size_t windowWidth : candidateWidths) {
        const std::size_t windowHeight = windowWidth * 5U / 4U;
        if (windowWidth < 32U || windowHeight >= height) {
            continue;
        }
        const std::size_t halfWidth = windowWidth / 2U;
        const std::size_t halfHeight = windowHeight / 2U;
        if (centerY < halfHeight || centerY + halfHeight >= height) {
            continue;
        }
        const std::size_t top = centerY - halfHeight;
        for (std::size_t centerX = halfWidth;
             centerX + halfWidth < width; centerX += scanStep) {
            const uint8_t score = scoreWindow(
                data, width, centerX - halfWidth, top,
                windowWidth, windowHeight);
            if (score > estimate.score) {
                estimate.score = score;
                estimate.centerX = static_cast<int16_t>(centerX);
            }
        }
    }

    estimate.detected = estimate.score >= detectionThreshold;
    if (!estimate.detected) {
        return estimate;
    }

    const int32_t imageCenter = static_cast<int32_t>(width / 2U);
    const int32_t offset = static_cast<int32_t>(estimate.centerX) - imageCenter;
    estimate.offsetPercent = static_cast<int8_t>(std::clamp<int32_t>(
        offset * 100 / imageCenter, -100, 100));
    if (estimate.offsetPercent < -10) {
        estimate.direction = FaceCandidateDirection::LEFT;
    } else if (estimate.offsetPercent > 10) {
        estimate.direction = FaceCandidateDirection::RIGHT;
    } else {
        estimate.direction = FaceCandidateDirection::CENTER;
    }
    return estimate;
}

const char* faceCandidateDirectionName(FaceCandidateDirection direction)
{
    switch (direction) {
        case FaceCandidateDirection::LEFT:
            return "L";
        case FaceCandidateDirection::CENTER:
            return "C";
        case FaceCandidateDirection::RIGHT:
            return "R";
        case FaceCandidateDirection::UNKNOWN:
            return "?";
    }
    return "?";
}

#include "CameraFrameAnalyzer.h"

#include <cstdint>

namespace {

CameraFrameStats finishStats(uint64_t sum, uint8_t minimum,
                             uint8_t maximum, uint32_t samples)
{
    CameraFrameStats stats;
    if (samples == 0) {
        return stats;
    }
    stats.valid = true;
    stats.meanLuma = static_cast<uint8_t>(sum / samples);
    stats.contrastRange = static_cast<uint8_t>(maximum - minimum);
    stats.sampledPixels = samples;
    return stats;
}

}  // namespace

CameraFrameStats analyzeRgb565(const uint8_t* data, std::size_t length,
                               std::size_t width, std::size_t height,
                               std::size_t sampleStridePixels)
{
    CameraFrameStats stats;
    if (data == nullptr || width == 0 || height == 0 ||
        sampleStridePixels == 0 || width > SIZE_MAX / height) {
        return stats;
    }

    const std::size_t pixelCount = width * height;
    if (pixelCount > SIZE_MAX / 2U || length < pixelCount * 2U) {
        return stats;
    }

    uint64_t lumaSum = 0;
    uint8_t minimumLuma = UINT8_MAX;
    uint8_t maximumLuma = 0;
    uint32_t samples = 0;
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount;
         pixelIndex += sampleStridePixels) {
        const std::size_t byteIndex = pixelIndex * 2U;
        const uint16_t pixel = static_cast<uint16_t>(data[byteIndex]) |
            static_cast<uint16_t>(data[byteIndex + 1U]) << 8U;
        const uint32_t red = ((pixel >> 11U) & 0x1FU) * 255U / 31U;
        const uint32_t green = ((pixel >> 5U) & 0x3FU) * 255U / 63U;
        const uint32_t blue = (pixel & 0x1FU) * 255U / 31U;
        const uint8_t luma = static_cast<uint8_t>(
            (red * 77U + green * 150U + blue * 29U) >> 8U);
        lumaSum += luma;
        if (luma < minimumLuma) {
            minimumLuma = luma;
        }
        if (luma > maximumLuma) {
            maximumLuma = luma;
        }
        ++samples;
    }

    return finishStats(lumaSum, minimumLuma, maximumLuma, samples);
}

CameraFrameStats analyzeGrayscale(const uint8_t* data, std::size_t length,
                                  std::size_t width, std::size_t height,
                                  std::size_t sampleStridePixels)
{
    CameraFrameStats stats;
    if (data == nullptr || width == 0 || height == 0 ||
        sampleStridePixels == 0 || width > SIZE_MAX / height) {
        return stats;
    }

    const std::size_t pixelCount = width * height;
    if (length < pixelCount) {
        return stats;
    }

    uint64_t lumaSum = 0;
    uint8_t minimumLuma = UINT8_MAX;
    uint8_t maximumLuma = 0;
    uint32_t samples = 0;
    for (std::size_t pixelIndex = 0; pixelIndex < pixelCount;
         pixelIndex += sampleStridePixels) {
        const uint8_t luma = data[pixelIndex];
        lumaSum += luma;
        if (luma < minimumLuma) {
            minimumLuma = luma;
        }
        if (luma > maximumLuma) {
            maximumLuma = luma;
        }
        ++samples;
    }
    return finishStats(lumaSum, minimumLuma, maximumLuma, samples);
}

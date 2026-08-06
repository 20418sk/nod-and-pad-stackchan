#pragma once

#include <cstddef>
#include <cstdint>

struct CameraFrameStats {
    bool valid{false};
    uint8_t meanLuma{0};
    uint8_t contrastRange{0};
    uint32_t sampledPixels{0};
};

// RGB565フレームを保存せず、間引いた画素から明るさ診断値だけを求める。
CameraFrameStats analyzeRgb565(const uint8_t* data, std::size_t length,
                               std::size_t width, std::size_t height,
                               std::size_t sampleStridePixels = 16);

// GC0308のグレースケールを1画素1バイトの明るさとして集計する。
CameraFrameStats analyzeGrayscale(const uint8_t* data, std::size_t length,
                                  std::size_t width, std::size_t height,
                                  std::size_t sampleStridePixels = 16);

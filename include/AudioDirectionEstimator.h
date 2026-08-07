#pragma once

#include <cstddef>
#include <cstdint>

// 内蔵デュアルマイクから求める大まかな到来方向。
// 顔や話者を識別する値ではなく、現在は診断表示だけに使う。
enum class SoundDirection : uint8_t {
    UNKNOWN,
    CENTER,
    LEFT,
    RIGHT,
};

struct AudioDirectionEstimate {
    SoundDirection direction{SoundDirection::UNKNOWN};
    int lagSamples{0};
    float correlation{0.0F};
    float confidence{0.0F};
};

class AudioDirectionEstimator {
public:
    AudioDirectionEstimate update(const int16_t* interleavedStereo,
                                  std::size_t frameCount);
    void reset();

    static const char* directionName(SoundDirection direction);

private:
    static float normalizedCorrelation(const int16_t* samples,
                                       std::size_t frameCount, int lag);
    static float clampFloat(float value, float minimum, float maximum);

    float smoothedDirectionScore_{0.0F};
    float smoothedConfidence_{0.0F};
};

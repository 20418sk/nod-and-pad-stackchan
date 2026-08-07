#pragma once

#include <cstddef>
#include <cstdint>

// A rough sound direction from the internal stereo microphones.
// The value does not identify a face or speaker. It is diagnostic only.
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

#pragma once

#include "AppConfig.h"
#include "AudioDirectionEstimator.h"

#include <cstddef>
#include <cstdint>

struct AudioMetrics {
    float rawRms{0.0F};
    float leftRms{0.0F};
    float rightRms{0.0F};
    float smoothedLevel{0.0F};
    float noiseFloor{app_config::audio::kInitialNoiseFloor};
    float startThreshold{0.0F};
    float endThreshold{0.0F};
    AudioDirectionEstimate direction{};
};

class AudioDetector {
public:
    bool begin();
    bool update(uint32_t nowMs, bool allowNoiseLearning,
                bool allowDirectionEstimation);

    void startCalibration(uint32_t nowMs, uint32_t durationMs);
    bool isCalibrating() const { return calibrating_; }
    bool takeCalibrationCompleted();
    bool healthy() const { return healthy_; }
    const AudioMetrics& metrics() const { return metrics_; }

private:
    void processBlock(const int16_t* samples, std::size_t frameCount,
                      uint32_t nowMs,
                      bool allowNoiseLearning,
                      bool allowDirectionEstimation);
    void calculateStereoRms(const int16_t* samples, std::size_t frameCount,
                            float& monoRms, float& leftRms,
                            float& rightRms) const;
    void updateThresholds();
    void updateCalibration(float rms, uint32_t nowMs);
    void finishCalibration();
    static float clampFloat(float value, float minimum, float maximum);

    int16_t captureBuffers_[app_config::audio::kCaptureBuffers]
                           [app_config::audio::kSamplesPerBlock *
                            app_config::audio::kStereoChannels]{};
    float calibrationValues_[app_config::audio::kCalibrationSamples]{};
    AudioMetrics metrics_{};
    AudioDirectionEstimator directionEstimator_{};

    std::size_t recordIndex_{2};
    std::size_t processIndex_{0};
    std::size_t primingBlocks_{2};
    std::size_t calibrationCount_{0};
    std::size_t calibrationWriteIndex_{0};
    uint32_t calibrationStartedMs_{0};
    uint32_t calibrationDurationMs_{0};
    uint8_t consecutiveCaptureFailures_{0};
    bool calibrating_{false};
    bool calibrationCompleted_{false};
    bool healthy_{false};
    bool levelInitialized_{false};
};

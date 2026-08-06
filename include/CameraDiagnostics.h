#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>

#include "FaceCandidateEstimator.h"

struct CameraDiagnosticInfo {
    bool initialized{false};
    bool healthy{false};
    bool frameReady{false};
    bool capturePending{false};
    uint16_t width{0};
    uint16_t height{0};
    uint8_t meanLuma{0};
    uint8_t contrastRange{0};
    uint32_t captureMs{0};
    uint32_t captureCount{0};
    uint32_t failureCount{0};
    int initError{0};
    FaceCandidateEstimate faceCandidate{};
};

// GC0308をサーボへ接続せず診断する。撮影は別タスクで行い、音声ループを塞がない。
class CameraDiagnostics {
public:
    bool begin();
    bool requestCapture(uint32_t nowMs);
    CameraDiagnosticInfo info() const;

private:
    static void taskEntry(void* argument);
    void taskLoop();
    void recordFailure(uint32_t captureMs);

    mutable portMUX_TYPE infoMux_ = portMUX_INITIALIZER_UNLOCKED;
    CameraDiagnosticInfo info_{};
    TaskHandle_t taskHandle_{nullptr};
    uint32_t lastRequestMs_{0};
    bool hasRequested_{false};
    std::atomic<bool> requestPending_{false};
};

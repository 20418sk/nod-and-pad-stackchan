#pragma once

#include <cstddef>
#include <cstdint>

enum class FaceCandidateDirection : uint8_t {
    UNKNOWN,
    LEFT,
    CENTER,
    RIGHT,
};

struct FaceCandidateEstimate {
    bool valid{false};
    bool detected{false};
    FaceCandidateDirection direction{FaceCandidateDirection::UNKNOWN};
    int16_t centerX{0};
    int8_t offsetPercent{0};
    uint8_t score{0};
};

// グレースケール画像から顔らしい左右対称な明暗配置を探す軽量診断。
// 本物の顔検出モデルではなく、サーボ指令には使用しない。
FaceCandidateEstimate estimateFaceCandidate(
    const uint8_t* data, std::size_t length, std::size_t width,
    std::size_t height, uint8_t detectionThreshold = 55);

const char* faceCandidateDirectionName(FaceCandidateDirection direction);

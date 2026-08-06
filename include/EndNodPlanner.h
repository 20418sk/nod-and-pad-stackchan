#pragma once

#include <cstdint>

struct EndNodPlan {
    uint8_t count{1};
    int targetPitch{120};
    int downSpeed{180};
    int returnSpeed{150};
    uint32_t downHoldMs{500};
    uint32_t betweenHoldMs{500};
    uint32_t finalHoldMs{850};
};

struct EndNodRandomValues {
    uint32_t depth{0};
    uint32_t target{0};
    uint32_t count{0};
    uint32_t downSpeed{0};
    uint32_t returnSpeed{0};
    uint32_t downHold{0};
    uint32_t betweenHold{0};
    uint32_t finalHold{0};
};

class EndNodPlanner {
public:
    EndNodPlan next(const EndNodRandomValues& randomValues);
    static bool isSafe(const EndNodPlan& plan);

private:
    static uint32_t mapToRange(uint32_t value, uint32_t minimum,
                               uint32_t maximum);
    static bool samePlan(const EndNodPlan& left, const EndNodPlan& right);

    EndNodPlan lastPlan_{};
    bool hasLastPlan_{false};
};

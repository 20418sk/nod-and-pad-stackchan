#include "EndNodPlanner.h"

uint32_t EndNodPlanner::mapToRange(uint32_t value, uint32_t minimum,
                                   uint32_t maximum)
{
    return minimum + (value % (maximum - minimum + 1U));
}

bool EndNodPlanner::samePlan(const EndNodPlan& left,
                             const EndNodPlan& right)
{
    return left.count == right.count &&
           left.targetPitch == right.targetPitch &&
           left.downSpeed == right.downSpeed &&
           left.returnSpeed == right.returnSpeed &&
           left.downHoldMs == right.downHoldMs &&
           left.betweenHoldMs == right.betweenHoldMs &&
           left.finalHoldMs == right.finalHoldMs;
}

EndNodPlan EndNodPlanner::next(const EndNodRandomValues& randomValues)
{
    EndNodPlan plan;
    const uint32_t depthRoll = mapToRange(randomValues.depth, 0, 99);
    if (depthRoll < 35) {
        plan.targetPitch = static_cast<int>(
            mapToRange(randomValues.target, 110, 130));
    } else if (depthRoll < 80) {
        plan.targetPitch = static_cast<int>(
            mapToRange(randomValues.target, 80, 100));
    } else {
        plan.targetPitch = static_cast<int>(
            mapToRange(randomValues.target, 50, 70));
    }

    // 公式推奨範囲の下限に近い深いうなずきは、負荷を抑えて1回だけにする。
    plan.count = plan.targetPitch <= 70
                     ? 1
                     : (mapToRange(randomValues.count, 0, 99) < 38 ? 2 : 1);
    plan.downSpeed = static_cast<int>(mapToRange(
        randomValues.downSpeed, 185, plan.targetPitch <= 70 ? 200 : 215));
    plan.returnSpeed = static_cast<int>(
        mapToRange(randomValues.returnSpeed, 165, 190));
    if (plan.returnSpeed >= plan.downSpeed) {
        plan.returnSpeed = plan.downSpeed - 10;
    }
    plan.downHoldMs = mapToRange(randomValues.downHold, 300, 450);
    plan.betweenHoldMs = mapToRange(randomValues.betweenHold, 330, 450);
    plan.finalHoldMs = mapToRange(randomValues.finalHold, 550, 750);

    if (hasLastPlan_ && samePlan(plan, lastPlan_)) {
        const int maximumDownSpeed = plan.targetPitch <= 70 ? 200 : 215;
        plan.downSpeed = plan.downSpeed >= maximumDownSpeed
                             ? plan.downSpeed - 1
                             : plan.downSpeed + 1;
    }
    lastPlan_ = plan;
    hasLastPlan_ = true;
    return plan;
}

bool EndNodPlanner::isSafe(const EndNodPlan& plan)
{
    const bool deep = plan.targetPitch <= 70;
    return (plan.count == 1 || plan.count == 2) &&
           plan.targetPitch >= 50 && plan.targetPitch <= 130 &&
           (!deep || plan.count == 1) &&
           plan.downSpeed >= 185 &&
           plan.downSpeed <= (deep ? 200 : 215) &&
           plan.returnSpeed >= 165 && plan.returnSpeed <= 190 &&
           plan.returnSpeed < plan.downSpeed &&
           plan.downHoldMs >= 300 && plan.downHoldMs <= 450 &&
           plan.betweenHoldMs >= 330 && plan.betweenHoldMs <= 450 &&
           plan.finalHoldMs >= 550 && plan.finalHoldMs <= 750;
}

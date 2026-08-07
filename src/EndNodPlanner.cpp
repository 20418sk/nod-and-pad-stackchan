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
            mapToRange(randomValues.target, 90, 110));
    } else if (depthRoll < 80) {
        plan.targetPitch = static_cast<int>(
            mapToRange(randomValues.target, 70, 90));
    } else {
        plan.targetPitch = static_cast<int>(
            mapToRange(randomValues.target, 50, 70));
    }

    // Use one nod near the lower official limit to reduce servo load.
    plan.count = plan.targetPitch <= 70
                     ? 1
                     : (mapToRange(randomValues.count, 0, 99) < 45 ? 2 : 1);
    plan.downSpeed = static_cast<int>(mapToRange(
        randomValues.downSpeed, 195, plan.targetPitch <= 70 ? 210 : 225));
    plan.returnSpeed = static_cast<int>(
        mapToRange(randomValues.returnSpeed, 175, 200));
    if (plan.returnSpeed >= plan.downSpeed) {
        plan.returnSpeed = plan.downSpeed - 10;
    }
    plan.downHoldMs = mapToRange(randomValues.downHold, 300, 420);
    plan.betweenHoldMs = mapToRange(randomValues.betweenHold, 420, 540);
    plan.finalHoldMs = mapToRange(randomValues.finalHold, 480, 650);

    if (hasLastPlan_ && samePlan(plan, lastPlan_)) {
        const int maximumDownSpeed = plan.targetPitch <= 70 ? 210 : 225;
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
           plan.targetPitch >= 50 && plan.targetPitch <= 110 &&
           (!deep || plan.count == 1) &&
           plan.downSpeed >= 195 &&
           plan.downSpeed <= (deep ? 210 : 225) &&
           plan.returnSpeed >= 175 && plan.returnSpeed <= 200 &&
           plan.returnSpeed < plan.downSpeed &&
           plan.downHoldMs >= 300 && plan.downHoldMs <= 420 &&
           plan.betweenHoldMs >= 420 && plan.betweenHoldMs <= 540 &&
           plan.finalHoldMs >= 480 && plan.finalHoldMs <= 650;
}

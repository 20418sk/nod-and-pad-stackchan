#pragma once

#include <cstddef>
#include <cstdint>

#include "EndNodPlanner.h"

class MotionController {
public:
    void begin(uint32_t nowMs);
    void update(uint32_t nowMs);

    bool listeningNod(uint32_t nowMs);
    bool endNod(const EndNodPlan& plan, uint32_t nowMs);
    bool headPetMotion(uint32_t nowMs);
    bool restorePitch(int pitchAngle, uint32_t nowMs);
    bool moveHome(uint32_t nowMs);
    bool sleepPose(uint32_t nowMs);

    int currentPitch() const;
    int currentYaw() const;

    bool isBusy() const { return busy_ || yawBusy_; }
    bool isReady() const { return ready_; }
    bool failed() const { return failed_; }
    bool isMicrophoneSuppressed(uint32_t nowMs) const;

private:
    struct MotionStep {
        int pitchAngle;
        uint32_t holdMs;
        int speed;
    };

    bool startSequence(const MotionStep* steps, std::size_t count,
                       uint32_t nowMs,
                       uint32_t suppressionAfterMs = 0);
    bool moveHomeAtSpeed(int speed, uint32_t moveDurationMs,
                         uint32_t nowMs);
    bool startPoseMove(int yawAngle, int pitchAngle, int speed,
                       uint32_t moveDurationMs,
                       uint32_t suppressionAfterMs, uint32_t nowMs);
    void commandCurrentStep();
    void commandPitchSafely(int pitchAngle, int speed);
    void commandYawSafely(int yawAngle, int speed);
    static int clampPitch(int pitchAngle);
    static int clampYaw(int yawAngle);
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs);

    MotionStep steps_[4]{};
    std::size_t stepCount_{0};
    std::size_t stepIndex_{0};
    uint32_t stepStartedMs_{0};
    uint32_t suppressionStartedMs_{0};
    uint32_t suppressionDurationMs_{0};
    uint32_t verificationStartedMs_{0};
    uint32_t lastVerificationPollMs_{0};
    uint32_t yawMoveStartedMs_{0};
    uint32_t yawMoveDurationMs_{0};
    bool busy_{false};
    bool yawBusy_{false};
    bool verifying_{false};
    bool ready_{false};
    bool failed_{false};
};

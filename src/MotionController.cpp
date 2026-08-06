#include "MotionController.h"

#include "AppConfig.h"

#include <M5StackChan.h>

#include <cstdlib>

bool MotionController::elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}

void MotionController::begin(uint32_t nowMs)
{
    // BSPの自動角度同期で現在角から滑らかに始め、停止後は自動でトルクを抜く。
    M5StackChan.Motion.setAutoAngleSyncEnabled(true);
    M5StackChan.Motion.setAutoTorqueReleaseEnabled(true);

    verifying_               = true;
    ready_                   = false;
    failed_                  = false;
    verificationStartedMs_   = nowMs;
    lastVerificationPollMs_  = nowMs - app_config::motion::kServoVerificationPollMs;

    const int currentPitch = M5StackChan.Motion.getCurrentYAngle();
    const int currentYaw = M5StackChan.Motion.getCurrentXAngle();
    const bool alreadyHome =
        std::abs(currentPitch - app_config::motion::kHomePitch) <=
            app_config::motion::kServoVerificationTolerance &&
        std::abs(currentYaw - app_config::motion::kHomeYaw) <=
            app_config::motion::kYawVerificationTolerance;
    if (alreadyHome) {
        verifying_ = false;
        ready_ = true;
        return;
    }

    // Startup verification is intentionally read-only.  On a cold power-up
    // the head may have relaxed to a slightly different angle; commanding a
    // correction here causes the audible, sudden "servo whirl" the user sees.
    // Normal interaction paths still call moveHome() when a deliberate return
    // to the safe home pose is required.
    verifying_ = false;
    ready_ = true;
}

void MotionController::update(uint32_t nowMs)
{
    if (busy_ && elapsed(nowMs, stepStartedMs_, steps_[stepIndex_].holdMs)) {
        ++stepIndex_;
        if (stepIndex_ < stepCount_) {
            stepStartedMs_ = nowMs;
            commandCurrentStep();
        } else {
            busy_ = false;
        }
    }

    if (yawBusy_ && elapsed(nowMs, yawMoveStartedMs_, yawMoveDurationMs_)) {
        yawBusy_ = false;
    }

    if (!verifying_ || failed_) {
        return;
    }

    if (elapsed(nowMs, lastVerificationPollMs_,
                app_config::motion::kServoVerificationPollMs)) {
        lastVerificationPollMs_ = nowMs;
        const int currentPitch = M5StackChan.Motion.getCurrentYAngle();
        const int currentYaw = M5StackChan.Motion.getCurrentXAngle();
        const bool pitchReady =
            std::abs(currentPitch - app_config::motion::kHomePitch) <=
            app_config::motion::kServoVerificationTolerance;
        const bool yawReady =
            std::abs(currentYaw - app_config::motion::kHomeYaw) <=
            app_config::motion::kYawVerificationTolerance;
        if (pitchReady && yawReady) {
            verifying_ = false;
            ready_     = true;
        }
    }

    if (verifying_ && elapsed(nowMs, verificationStartedMs_,
                              app_config::motion::kServoVerificationTimeoutMs)) {
        verifying_ = false;
        ready_     = false;
        failed_    = true;
        busy_      = false;
        yawBusy_   = false;
        // 通信不良時は追加の角度指令を止め、可能ならトルクも解放する。
        M5StackChan.Motion.setTorqueEnabled(false);
    }
}

bool MotionController::listeningNod(uint32_t nowMs)
{
    const MotionStep sequence[] = {
        {app_config::motion::kHomePitch - app_config::motion::kListeningNodDepth,
         180, app_config::motion::kListeningNodSpeed},
        {app_config::motion::kHomePitch, 360,
         app_config::motion::kListeningNodSpeed},
    };
    return startSequence(sequence, 2, nowMs);
}

bool MotionController::endNod(const EndNodPlan& plan, uint32_t nowMs)
{
    const int safeTarget = clampPitch(plan.targetPitch);
    MotionStep sequence[4] = {
        {safeTarget, plan.downHoldMs, plan.downSpeed},
        {app_config::motion::kHomePitch,
         plan.count == 2 ? plan.betweenHoldMs : plan.finalHoldMs,
         plan.returnSpeed},
        {safeTarget, plan.downHoldMs, plan.downSpeed},
        {app_config::motion::kHomePitch, plan.finalHoldMs, plan.returnSpeed},
    };
    return startSequence(sequence, plan.count == 2 ? 4 : 2, nowMs);
}

bool MotionController::headPetMotion(uint32_t nowMs)
{
    // 公式版は複数の上下・左右動作から選ぶ。本作品はMVPの安全要件に従い、
    // 現在位置から小さく上を向く上下軸動作だけを残す。
    const MotionStep sequence[] = {
        {currentPitch() + app_config::motion::kHeadPetRise,
         500, app_config::motion::kHeadPetSpeed},
    };
    return startSequence(sequence, 1, nowMs);
}

bool MotionController::restorePitch(int pitchAngle, uint32_t nowMs)
{
    const MotionStep sequence[] = {
        {pitchAngle, 650, app_config::motion::kHeadPetSpeed},
    };
    return startSequence(sequence, 1, nowMs);
}

bool MotionController::moveHome(uint32_t nowMs)
{
    return moveHomeAtSpeed(app_config::motion::kHomeSpeed, 1000, nowMs);
}

bool MotionController::moveHomeAtSpeed(int speed, uint32_t moveDurationMs,
                                       uint32_t nowMs)
{
    const MotionStep sequence[] = {
        {app_config::motion::kHomePitch, moveDurationMs, speed},
    };
    if (!startSequence(sequence, 1, nowMs)) {
        return false;
    }
    // 全動作の基準を必ず物理ホームへ合わせる。BSPのgoHome()は上下も0へ
    // 動かすため使わず、作品側の安全な上下ホームと横0度を個別に指令する。
    commandYawSafely(app_config::motion::kHomeYaw,
                     speed);
    return true;
}

bool MotionController::sleepPose(uint32_t nowMs)
{
    return startPoseMove(app_config::motion::kHomeYaw,
                         app_config::motion::kSleepPitch,
                         app_config::motion::kHomeSpeed, 850,
                         app_config::motion::kServoSoundSuppressionAfterMs,
                         nowMs);
}

int MotionController::currentPitch() const
{
    if (failed_) {
        return app_config::motion::kHomePitch;
    }
    return clampPitch(M5StackChan.Motion.getCurrentYAngle());
}

int MotionController::currentYaw() const
{
    if (failed_) {
        return app_config::motion::kHomeYaw;
    }
    return clampYaw(M5StackChan.Motion.getCurrentXAngle());
}

bool MotionController::startSequence(const MotionStep* steps, std::size_t count,
                                     uint32_t nowMs,
                                     uint32_t suppressionAfterMs)
{
    if (failed_ || isBusy() || steps == nullptr || count == 0 || count > 4) {
        return false;
    }

    uint32_t totalDuration = 0;
    for (std::size_t i = 0; i < count; ++i) {
        steps_[i] = steps[i];
        steps_[i].pitchAngle = clampPitch(steps_[i].pitchAngle);
        totalDuration += steps_[i].holdMs;
    }

    stepCount_              = count;
    stepIndex_              = 0;
    stepStartedMs_          = nowMs;
    suppressionStartedMs_   = nowMs;
    const uint32_t afterMs = suppressionAfterMs == 0
                                 ? app_config::motion::kServoSoundSuppressionAfterMs
                                 : suppressionAfterMs;
    suppressionDurationMs_  = totalDuration + afterMs;
    busy_                   = true;
    commandCurrentStep();
    return true;
}

bool MotionController::startPoseMove(int yawAngle, int pitchAngle, int speed,
                                     uint32_t moveDurationMs,
                                     uint32_t suppressionAfterMs,
                                     uint32_t nowMs)
{
    if (failed_ || isBusy() || moveDurationMs == 0) {
        return false;
    }

    steps_[0] = {clampPitch(pitchAngle), moveDurationMs, speed};
    stepCount_ = 1;
    stepIndex_ = 0;
    stepStartedMs_ = nowMs;
    yawMoveStartedMs_ = nowMs;
    yawMoveDurationMs_ = moveDurationMs;
    suppressionStartedMs_ = nowMs;
    suppressionDurationMs_ = moveDurationMs + suppressionAfterMs;
    busy_ = true;
    yawBusy_ = true;
    commandCurrentStep();
    commandYawSafely(yawAngle, speed);
    return true;
}

void MotionController::commandCurrentStep()
{
    commandPitchSafely(steps_[stepIndex_].pitchAngle, steps_[stepIndex_].speed);
}

void MotionController::commandPitchSafely(int pitchAngle, int speed)
{
    const int safePitch = clampPitch(pitchAngle);
    int safeSpeed = speed;
    if (safeSpeed < 0) {
        safeSpeed = 0;
    } else if (safeSpeed > 1000) {
        safeSpeed = 1000;
    }
    M5StackChan.Motion.moveY(safePitch, safeSpeed);
}

void MotionController::commandYawSafely(int yawAngle, int speed)
{
    const int safeYaw = clampYaw(yawAngle);
    int safeSpeed = speed;
    if (safeSpeed < 0) {
        safeSpeed = 0;
    } else if (safeSpeed > 1000) {
        safeSpeed = 1000;
    }
    M5StackChan.Motion.moveX(safeYaw, safeSpeed);
}

int MotionController::clampPitch(int pitchAngle)
{
    // 作品側安全範囲は公式5～85度より狭い。二重制限で設定ミスも防ぐ。
    int safePitch = pitchAngle;
    if (safePitch < app_config::motion::kWorkPitchMin) {
        safePitch = app_config::motion::kWorkPitchMin;
    } else if (safePitch > app_config::motion::kWorkPitchMax) {
        safePitch = app_config::motion::kWorkPitchMax;
    }
    if (safePitch < app_config::motion::kOfficialPitchMin) {
        safePitch = app_config::motion::kOfficialPitchMin;
    } else if (safePitch > app_config::motion::kOfficialPitchMax) {
        safePitch = app_config::motion::kOfficialPitchMax;
    }
    return safePitch;
}

int MotionController::clampYaw(int yawAngle)
{
    int safeYaw = yawAngle;
    if (safeYaw < app_config::motion::kWorkYawMin) {
        safeYaw = app_config::motion::kWorkYawMin;
    } else if (safeYaw > app_config::motion::kWorkYawMax) {
        safeYaw = app_config::motion::kWorkYawMax;
    }
    if (safeYaw < app_config::motion::kOfficialYawMin) {
        safeYaw = app_config::motion::kOfficialYawMin;
    } else if (safeYaw > app_config::motion::kOfficialYawMax) {
        safeYaw = app_config::motion::kOfficialYawMax;
    }
    return safeYaw;
}

bool MotionController::isMicrophoneSuppressed(uint32_t nowMs) const
{
    if (suppressionDurationMs_ == 0) {
        return false;
    }
    return !elapsed(nowMs, suppressionStartedMs_, suppressionDurationMs_);
}

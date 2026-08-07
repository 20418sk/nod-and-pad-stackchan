#include <Arduino.h>
#include <M5StackChan.h>
#include <esp_system.h>

#include "AppConfig.h"
#include "AudioDetector.h"
#include "EndNodPlanner.h"
#include "FaceRenderer.h"
#include "HeadPetController.h"
#include "HeadPetGestureDetector.h"
#include "HeadTouchAudioGuard.h"
#include "ListenerStateMachine.h"
#include "MotionController.h"
#include "ScreenTouchMapper.h"

namespace {

class Application {
public:
    void begin()
    {
        Serial.begin(115200);
        Serial.println("\n[Nod & Pat Stack-chan] 起動します");

        // StackChan-BSP::begin() also initializes M5Unified through M5.begin().
        // The application uses local hardware only. It starts no Wi-Fi, BLE, or network client.
        M5StackChan.begin();

        const uint32_t nowMs = millis();
        const bool displayOk = faceRenderer_.begin(M5StackChan.Display());
        stateMachine_.begin(nowMs);

        FaceDebugInfo initialDebug;
        initialDebug.stateName = ListenerStateMachine::stateName(stateMachine_.state());
        faceRenderer_.render(FaceExpression::NORMAL, initialDebug, nowMs, true);
        faceRenderer_.showStartupCheck();

        const bool audioOk = audioDetector_.begin();
        motionController_.begin(millis());
        setLed(0, 0, 0);

        if (!displayOk) {
            setFatalError("DISPLAY ERROR");
        } else if (!audioOk) {
            setFatalError("MIC ERROR");
        }
    }

    void update()
    {
        M5StackChan.update();
        const uint32_t nowMs = millis();

        motionController_.update(nowMs);
        updateScreenLook(nowMs);
        updateCalibrationUi(nowMs);
        tryStartPendingReaction(nowMs);
        handleTouch(nowMs);
        handleHeadPet(nowMs);

        if (motionController_.failed() && !fatalError_) {
            setFatalError("SERVO ERROR");
        }

        if (pendingManualCalibration_ && !fatalError_ &&
            audioDetector_.healthy() && motionController_.isReady() &&
            !motionController_.isBusy() &&
            !headPetController_.active() &&
            !headTouchAudioGuard_.suppressed() &&
            !motionController_.isMicrophoneSuppressed(nowMs)) {
            pendingManualCalibration_ = false;
            beginCalibration(nowMs, app_config::audio::kManualCalibrationMs,
                             false);
            Serial.println("[音] 再キャリブレーション開始");
        }

        // Wait for servo verification and the quiet period.
        // Microphone calibration starts only after the user taps the screen.
        if (!startupServoTestCompleteShown_ && !fatalError_ &&
            audioDetector_.healthy() &&
            motionController_.isReady() &&
            !headPetController_.active() &&
            !headTouchAudioGuard_.suppressed() &&
            !motionController_.isMicrophoneSuppressed(nowMs)) {
            startupServoTestCompleteShown_ = true;
            startupGuidePage_ = 1;
            faceRenderer_.showStartupGuide(startupGuidePage_);
            Serial.println("[サーボ] 起動時テスト完了");
        }

        const ListenerState currentState = stateMachine_.state();
        const bool servoSuppressed =
            motionController_.isMicrophoneSuppressed(nowMs);
        const bool noiseLearningAllowed =
            (currentState == ListenerState::IDLE || currentState == ListenerState::SLEEPING) &&
            !servoSuppressed &&
            !audioDetector_.isCalibrating() && startupGuidePage_ == 0 &&
            !headPetController_.active() &&
            !headTouchAudioGuard_.suppressed();
        const bool directionEstimationAllowed =
            !servoSuppressed && !motionController_.isBusy() &&
            motionController_.isReady() && !audioDetector_.isCalibrating() &&
            startupGuidePage_ == 0 &&
            !headPetController_.active() &&
            !headTouchAudioGuard_.suppressed();

        const bool newAudioBlock = audioDetector_.update(
            nowMs, noiseLearningAllowed, directionEstimationAllowed);
        if (!audioDetector_.healthy() && !fatalError_) {
            setFatalError("MIC ERROR");
        }

        if (audioDetector_.takeCalibrationCompleted()) {
            if (manualCalibrationUi_) {
                manualCalibrationUi_ = false;
                calibrationUiActive_ = false;
                faceRenderer_.clearOverlay();
                stateMachine_.resetToIdle(nowMs);
                setLed(0, 0, 0);
                Serial.println("[音] 再キャリブレーション完了");
            } else {
                calibrationUiActive_ = false;
                startupGuidePage_ = 2;
                faceRenderer_.showStartupGuide(startupGuidePage_);
                Serial.println("[音] 起動時キャリブレーション完了");
            }
        }

        if (fatalError_) {
            // During a fatal error, update only the BSP and any safe home motion.
            yield();
            return;
        }

        const AudioMetrics& metrics = audioDetector_.metrics();
        ListenerInput input;
        input.sampleAvailable    = newAudioBlock;
        input.audioReady         = audioDetector_.healthy() &&
                                   !audioDetector_.isCalibrating() &&
                                   motionController_.isReady();
        input.audioHealthy       = audioDetector_.healthy();
        input.detectionSuppressed = audioDetector_.isCalibrating() ||
                                    motionController_.isMicrophoneSuppressed(nowMs) ||
                                    startupGuidePage_ != 0 ||
                                    headPetController_.active() ||
                                    headTouchAudioGuard_.suppressed();
        input.reactionComplete   = !motionController_.isBusy();
        input.level              = metrics.smoothedLevel;
        input.startThreshold     = metrics.startThreshold;
        input.endThreshold       = metrics.endThreshold;

        ListenerOutput output = stateMachine_.update(nowMs, input);
        if (output.stateChanged &&
            output.previousState == ListenerState::REACTING &&
            output.state == ListenerState::COOLDOWN) {
            if (lastStartedReaction_ == ReactionType::NORMAL_NOD) {
                faceRenderer_.startNodAfterglow(nowMs);
            }
            lastStartedReaction_ = ReactionType::NONE;
        }
        if (!headPetController_.active()) {
            handleListenerOutput(output, nowMs);
        }
        const FaceExpression expression = expressionForState();
        const FaceDebugInfo debugInfo = makeDebugInfo();
        // Do not redraw when a state change keeps the same expression.
        // This prevents full-screen flicker when the level moves around a threshold.
        faceRenderer_.render(expression, debugInfo, nowMs);
        updateLedForState();

        if (output.stateChanged) {
            Serial.printf("[状態] %s -> %s\n",
                          ListenerStateMachine::stateName(output.previousState),
                          ListenerStateMachine::stateName(stateMachine_.state()));
        }
    }

private:
    static bool elapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs)
    {
        return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
    }

    EndNodPlan randomEndNodPlan()
    {
        const EndNodRandomValues values{
            esp_random(), esp_random(), esp_random(), esp_random(),
            esp_random(), esp_random(), esp_random(), esp_random()};
        return endNodPlanner_.next(values);
    }

    void beginCalibration(uint32_t nowMs, uint32_t durationMs, bool startup)
    {
        calibrationUiActive_ = true;
        calibrationUiStartup_ = startup;
        calibrationUiStartedMs_ = nowMs;
        calibrationUiDurationMs_ = durationMs;
        calibrationUiLastSecond_ = 0;
        faceRenderer_.showCalibration(
            startup, static_cast<uint8_t>((durationMs + 999U) / 1000U));
        audioDetector_.startCalibration(nowMs, durationMs);
    }

    void updateCalibrationUi(uint32_t nowMs)
    {
        if (!calibrationUiActive_ || !audioDetector_.isCalibrating()) {
            return;
        }
        const uint32_t age = static_cast<uint32_t>(nowMs - calibrationUiStartedMs_);
        const uint32_t remainingMs = age >= calibrationUiDurationMs_
                                          ? 1U
                                          : calibrationUiDurationMs_ - age;
        const uint8_t remainingSeconds = static_cast<uint8_t>(
            (remainingMs + 999U) / 1000U);
        if (remainingSeconds != calibrationUiLastSecond_) {
            calibrationUiLastSecond_ = remainingSeconds;
            faceRenderer_.showCalibration(calibrationUiStartup_,
                                           remainingSeconds);
        }
    }

    void handleTouch(uint32_t nowMs)
    {
        int16_t touchX = 0;
        int16_t touchY = 0;
        const bool touching = M5StackChan.Display().getTouch(&touchX, &touchY);
        if (touching && !touching_) {
            touching_        = true;
            longPressHandled_ = false;
            touchStartedMs_   = nowMs;
            touchStartedX_    = touchX;
            const int width = M5StackChan.Display().width();
            const int height = M5StackChan.Display().height();
            touchStartedInDebugCorner_ = ScreenTouchMapper::isDebugCorner(
                touchX, touchY, width, height);
            touchStartedInCalibrationArea_ =
                ScreenTouchMapper::isCalibrationArea(
                    touchX, touchY, width, height);
            return;
        }

        if (touching && touching_ && !longPressHandled_ &&
            startupGuidePage_ == 0 && touchStartedInDebugCorner_ &&
            elapsed(nowMs, touchStartedMs_,
                    app_config::touch::kDebugLongPressMs)) {
            longPressHandled_ = true;
            if (!fatalError_ && !manualCalibrationUi_) {
                faceRenderer_.setDebugEnabled(!faceRenderer_.debugEnabled());
                faceRenderer_.render(expressionForState(), makeDebugInfo(),
                                     nowMs, true);
                Serial.printf("[display] debug %s\n",
                              faceRenderer_.debugEnabled() ? "ON" : "OFF");
            }
            return;
        }

        if (touching && touching_ && !longPressHandled_ &&
            startupGuidePage_ == 0 && touchStartedInCalibrationArea_ &&
            elapsed(nowMs, touchStartedMs_, app_config::touch::kLongPressMs)) {
            longPressHandled_ = true;
            if (!fatalError_ && !motionController_.isBusy() &&
                audioDetector_.healthy() &&
                !headPetController_.active() &&
                stateMachine_.state() != ListenerState::STARTUP) {
                const bool wasSleeping =
                    stateMachine_.state() == ListenerState::SLEEPING;
                stateMachine_.resetToIdle(nowMs);
                manualCalibrationUi_ = true;
                faceRenderer_.showCalibration(false, 3);
                setLed(0, 0, 5);

                if (wasSleeping) {
                    if (!motionController_.moveHome(nowMs)) {
            setFatalError("SERVO ERROR");
                        return;
                    }
                }

                if (motionController_.isMicrophoneSuppressed(nowMs)) {
                    pendingManualCalibration_ = true;
                    Serial.println("[音] サーボ静音待ち");
                } else {
                    beginCalibration(nowMs,
                                     app_config::audio::kManualCalibrationMs,
                                     false);
                    Serial.println("[音] 再キャリブレーション開始");
                }
            }
            return;
        }

        if (!touching && touching_) {
            const uint32_t heldMs = static_cast<uint32_t>(nowMs - touchStartedMs_);
            touching_ = false;
            if (!longPressHandled_ &&
                heldMs >= app_config::touch::kDebounceMs &&
                startupGuidePage_ != 0) {
                if (startupGuidePage_ == 1) {
                    startupGuidePage_ = 0;
                    beginCalibration(nowMs,
                                     app_config::audio::kStartupCalibrationMs,
                                     true);
                    Serial.println("[音] 起動時キャリブレーション開始");
                } else if (startupGuidePage_ < 4) {
                    ++startupGuidePage_;
                    faceRenderer_.showStartupGuide(startupGuidePage_);
                } else {
                    if (!motionController_.settleToHome(nowMs)) {
                        setFatalError("SERVO ERROR");
                        return;
                    }
                    startupGuidePage_ = 0;
                    faceRenderer_.clearOverlay();
                    faceRenderer_.render(expressionForState(), makeDebugInfo(),
                                         nowMs, true);
                }
                return;
            }
            if (!longPressHandled_ &&
                heldMs >= app_config::touch::kDebounceMs &&
                heldMs <= app_config::touch::kShortTapMaximumMs) {
                handleScreenTap(nowMs);
                return;
            }
        }
    }

    void handleScreenTap(uint32_t nowMs)
    {
        if (fatalError_ || manualCalibrationUi_ || startupGuidePage_ != 0 ||
            !audioDetector_.healthy() || !motionController_.isReady() ||
            stateMachine_.state() != ListenerState::IDLE) {
            return;
        }

        const bool canQueueDuringMotion =
            headPetController_.active() || screenLookActive_;
        if (motionController_.isBusy() && !canQueueDuringMotion) {
            return;
        }

        const ScreenTouchRegion region = ScreenTouchMapper::horizontalRegion(
            touchStartedX_, M5StackChan.Display().width());
        if (region == ScreenTouchRegion::CENTER) {
            if (!physicalPetSession_) {
                screenBoopRequested_ = true;
            }
            return;
        }

        const int nextYaw = ScreenTouchMapper::steppedYawTarget(
            screenYawTarget_, region,
            app_config::motion::kScreenTouchYawStep,
            app_config::motion::kScreenTouchYawMax);
        if (nextYaw != screenYawTarget_) {
            screenYawTarget_ = nextYaw;
            if (motionController_.isBusy()) {
                pendingScreenYawMove_ = true;
            } else if (!startScreenYawMove(nowMs)) {
                return;
            }
        }
        if (!physicalPetSession_) {
            screenBoopRequested_ = true;
        }
        screenLookActive_ = true;
        screenLookStartedMs_ = nowMs;
    }

    bool startScreenYawMove(uint32_t nowMs)
    {
        if (!motionController_.lookTowardScreenTouch(screenYawTarget_, nowMs)) {
            setFatalError("SERVO ERROR");
            return false;
        }
        pendingScreenYawMove_ = false;
        screenLookActive_ = true;
        screenLookStartedMs_ = nowMs;
        return true;
    }

    void updateScreenLook(uint32_t nowMs)
    {
        if (pendingScreenYawMove_) {
            if (!fatalError_ && !manualCalibrationUi_ &&
                startupGuidePage_ == 0 && motionController_.isReady() &&
                !motionController_.isBusy() &&
                stateMachine_.state() == ListenerState::IDLE) {
                startScreenYawMove(nowMs);
            }
            return;
        }

        if (!screenLookActive_ ||
            !elapsed(nowMs, screenLookStartedMs_,
                     app_config::motion::kScreenTouchLookDurationMs) ||
            fatalError_ || manualCalibrationUi_ || startupGuidePage_ != 0 ||
            headPetController_.active() || motionController_.isBusy() ||
            stateMachine_.state() != ListenerState::IDLE) {
            return;
        }

        if (!motionController_.returnYawHome(nowMs)) {
            setFatalError("SERVO ERROR");
            return;
        }
        screenYawTarget_ = app_config::motion::kHomeYaw;
        pendingScreenYawMove_ = false;
        screenLookActive_ = false;
    }

    void handleHeadPet(uint32_t nowMs)
    {
        const bool screenBoop = screenBoopRequested_;
        screenBoopRequested_ = false;
        auto& touchSensor = M5StackChan.TouchSensor;
        const auto intensities = touchSensor.getIntensities();
        const bool swiped = headPetGestureDetector_.update(
            nowMs, intensities);
        const bool released = touchSensor.wasReleased();
        const bool anyHeadTouch =
            intensities[0] > 0 || intensities[1] > 0 || intensities[2] > 0;
        if (headTouchAudioGuard_.update(nowMs, anyHeadTouch || released)) {
            if (stateMachine_.state() != ListenerState::STARTUP) {
                stateMachine_.resetToIdle(nowMs);
            }
            pendingReaction_ = ReactionType::NONE;
            lastStartedReaction_ = ReactionType::NONE;
            Serial.println("[頭部タッチ] 機械音の判定抑制を開始");
        }
        const bool acceptsNewSwipe = !fatalError_ && !manualCalibrationUi_ &&
                                     startupGuidePage_ == 0 &&
                                     !calibrationUiActive_ &&
                                     !audioDetector_.isCalibrating() &&
                                     stateMachine_.state() !=
                                         ListenerState::STARTUP &&
                                     audioDetector_.healthy() &&
                                     motionController_.isReady();

        const bool acceptedGesture = acceptsNewSwipe && (swiped || screenBoop);
        const HeadPetUpdate petUpdate = headPetController_.update(
            nowMs, acceptedGesture, released || screenBoop);

        if (petUpdate.entered) {
            pitchBeforePet_ = motionController_.currentPitch();
            pendingPetRestore_ = false;
            petMotionUsed_ = false;
            pendingPetMotion_ = false;
            // Do not treat head-contact noise as speech after the touch reaction ends.
            stateMachine_.resetToIdle(nowMs);
            lastStartedReaction_ = ReactionType::NONE;
        }

        if (petUpdate.swipeAccepted) {
            const HeadPetGestureType gestureType =
                screenBoop ? HeadPetGestureType::SINGLE_TAP
                           : headPetGestureDetector_.lastGestureType();
            if (!screenBoop) {
                physicalPetSession_ = true;
            }
            faceRenderer_.setPettingTapStyle(
                gestureType == HeadPetGestureType::SINGLE_TAP);
            if (gestureType == HeadPetGestureType::SWIPE) {
                if (motionController_.isBusy()) {
                    // Keep a head swipe received during yaw motion.
                    // Add the pitch motion after the current yaw motion ends.
                    pendingPetMotion_ = true;
                } else {
                    petMotionUsed_ = motionController_.headPetMotion(nowMs) ||
                                     petMotionUsed_;
                }
            }
            Serial.println(gestureType == HeadPetGestureType::SINGLE_TAP
                               ? "[頭部タッチ] 1回タップを検出"
                               : "[なでなで] 頭部スワイプを検出");
        }

        if (pendingPetMotion_ && headPetController_.active() && !fatalError_ &&
            motionController_.isReady() && !motionController_.isBusy()) {
            if (motionController_.headPetMotion(nowMs)) {
                pendingPetMotion_ = false;
                petMotionUsed_ = true;
            }
        }

        if (petUpdate.restored) {
            pendingPetMotion_ = false;
            pendingPetRestore_ = petMotionUsed_;
            petMotionUsed_ = false;
            physicalPetSession_ = false;
        }

        if (pendingPetRestore_ && !fatalError_ &&
            motionController_.isReady() && !motionController_.isBusy()) {
            if (motionController_.restorePitch(pitchBeforePet_, nowMs)) {
                pendingPetRestore_ = false;
                Serial.println("[なでなで] 元の姿勢へ復帰");
            }
        }
    }

    void handleListenerOutput(const ListenerOutput& output, uint32_t nowMs)
    {
        if (output.wokeFromSleep) {
            // Suppress speech decisions while returning from sleep to the safe home.
            // The wake sound must not also become a completed speech event.
            if (!motionController_.moveHome(nowMs)) {
            setFatalError("SERVO ERROR");
                return;
            }
            stateMachine_.resetToIdle(nowMs);
            return;
        }

        if (output.stateChanged && output.state == ListenerState::SLEEPING) {
            if (!motionController_.sleepPose(nowMs)) {
            setFatalError("SERVO ERROR");
                return;
            }
        }

        if (output.stateChanged && output.state == ListenerState::LISTENING) {
            // Change only the face when speech starts.
            // No servo motion means a short voice is not lost during servo-noise suppression.
            const AudioDirectionEstimate& direction =
                audioDetector_.metrics().direction;
            Serial.printf("[方向] %s lag=%d corr=%.2f conf=%.2f\n",
                          AudioDirectionEstimator::directionName(
                              direction.direction),
                          direction.lagSamples,
                          static_cast<double>(direction.correlation),
                          static_cast<double>(direction.confidence));
        }

        if (!output.reactionRequested) {
            return;
        }

        pendingNodPlan_ = randomEndNodPlan();
        Serial.printf("[反応] 発話=%lums 回数=%u 最下点=%.1f度 速度=%d/%d\n",
                      static_cast<unsigned long>(output.completedSpeechMs),
                      static_cast<unsigned>(pendingNodPlan_.count),
                      static_cast<double>(pendingNodPlan_.targetPitch) / 10.0,
                      pendingNodPlan_.downSpeed,
                      pendingNodPlan_.returnSpeed);
        pendingReaction_ = ReactionType::NORMAL_NOD;
        tryStartPendingReaction(nowMs);
    }

    void tryStartPendingReaction(uint32_t nowMs)
    {
        if (pendingReaction_ == ReactionType::NONE ||
            motionController_.isBusy() || headPetController_.active()) {
            return;
        }

        bool started = false;
        switch (pendingReaction_) {
            case ReactionType::NORMAL_NOD:
                started = motionController_.endNod(pendingNodPlan_, nowMs);
                break;
            case ReactionType::NONE:
                break;
        }

        if (!started) {
            setFatalError("SERVO ERROR");
        } else {
            lastStartedReaction_ = pendingReaction_;
        }
        pendingReaction_ = ReactionType::NONE;
    }

    FaceExpression expressionForState() const
    {
        if (headPetController_.active()) {
            return (headPetController_.decorated() ||
                    headPetGestureDetector_.contactActive())
                       ? FaceExpression::PETTING
                       : FaceExpression::NODDING;
        }

        switch (stateMachine_.state()) {
            case ListenerState::STARTUP:
            case ListenerState::IDLE:
            case ListenerState::COOLDOWN:
            case ListenerState::SPEECH_CANDIDATE:
                return FaceExpression::NORMAL;
            case ListenerState::LISTENING:
            case ListenerState::END_CANDIDATE:
                return FaceExpression::LISTENING;
            case ListenerState::REACTING:
                return FaceExpression::NODDING;
            case ListenerState::SLEEPING:
                return FaceExpression::SLEEPING;
        }
        return FaceExpression::ERROR;
    }

    FaceDebugInfo makeDebugInfo() const
    {
        const AudioMetrics& metrics = audioDetector_.metrics();
        FaceDebugInfo info;
        info.stateName        = ListenerStateMachine::stateName(stateMachine_.state());
        info.directionName    = AudioDirectionEstimator::directionName(
            metrics.direction.direction);
        info.smoothedLevel    = metrics.smoothedLevel;
        info.leftLevel        = metrics.leftRms;
        info.rightLevel       = metrics.rightRms;
        info.noiseFloor       = metrics.noiseFloor;
        info.dynamicThreshold = metrics.startThreshold;
        info.directionConfidence = metrics.direction.confidence;
        info.directionLagSamples = metrics.direction.lagSamples;
        info.yawTarget        = motionController_.currentYaw();
        info.lastSpeechMs     = stateMachine_.lastSpeechDurationMs();
        return info;
    }

    void updateLedForState()
    {
        if (manualCalibrationUi_) {
            return;
        }

        const ListenerState state = stateMachine_.state();
        const bool petting = headPetController_.active();
        if (state == lastLedState_ && petting == lastLedPetting_) {
            return;
        }
        lastLedState_   = state;
        lastLedPetting_ = petting;

        // Keep brightness well below the official example value of 168.
        // The selected low values still show a clear difference on the device.
        if (petting) {
            setLed(16, 7, 8);
            return;
        }

        switch (state) {
            case ListenerState::SPEECH_CANDIDATE:
                setLed(0, 8, 1);
                break;
            case ListenerState::LISTENING:
                setLed(0, 26, 2);
                break;
            case ListenerState::END_CANDIDATE:
                setLed(0, 8, 1);
                break;
            default:
                setLed(0, 0, 0);
                break;
        }
    }

    void setLed(uint8_t red, uint8_t green, uint8_t blue)
    {
        // Use the BSP RGB API with low values to avoid continuous high brightness.
        M5StackChan.showRgbColor(red, green, blue);
    }

    void setFatalError(const char* message)
    {
        if (fatalError_) {
            return;
        }
        fatalError_ = true;
        setLed(14, 0, 0);
        faceRenderer_.clearOverlay();
        faceRenderer_.showError(message);
        Serial.printf("[致命的エラー] %s\n", message);
    }

    AudioDetector audioDetector_;
    ListenerStateMachine stateMachine_;
    MotionController motionController_;
    EndNodPlanner endNodPlanner_;
    FaceRenderer faceRenderer_;
    HeadPetController headPetController_{
        app_config::head_pet::kRestoreDelayMs,
        app_config::head_pet::kDecorationDurationMs};
    HeadPetGestureDetector headPetGestureDetector_{
        app_config::head_pet::kGestureMinimumMoveMs,
        app_config::head_pet::kGestureMaximumMs,
        app_config::head_pet::kGestureReleaseResetMs,
        app_config::head_pet::kTapMinimumContactMs,
        app_config::head_pet::kTapMaximumContactMs};
    HeadTouchAudioGuard headTouchAudioGuard_{
        app_config::head_pet::kContactAudioSuppressionMs};

    ListenerState lastLedState_{ListenerState::STARTUP};
    int pitchBeforePet_{app_config::motion::kHomePitch};
    int screenYawTarget_{app_config::motion::kHomeYaw};
    uint32_t touchStartedMs_{0};
    uint32_t screenLookStartedMs_{0};
    int16_t touchStartedX_{0};
    bool touching_{false};
    bool longPressHandled_{false};
    bool touchStartedInDebugCorner_{false};
    bool touchStartedInCalibrationArea_{false};
    bool screenLookActive_{false};
    bool screenBoopRequested_{false};
    bool pendingScreenYawMove_{false};
    bool physicalPetSession_{false};
    bool manualCalibrationUi_{false};
    bool pendingManualCalibration_{false};
    bool calibrationUiActive_{false};
    bool calibrationUiStartup_{false};
    uint32_t calibrationUiStartedMs_{0};
    uint32_t calibrationUiDurationMs_{0};
    uint8_t calibrationUiLastSecond_{0};
    bool startupServoTestCompleteShown_{false};
    uint8_t startupGuidePage_{0};
    bool fatalError_{false};
    bool pendingPetRestore_{false};
    bool pendingPetMotion_{false};
    bool petMotionUsed_{false};
    ReactionType pendingReaction_{ReactionType::NONE};
    ReactionType lastStartedReaction_{ReactionType::NONE};
    EndNodPlan pendingNodPlan_{};
    bool lastLedPetting_{false};
};

Application application;

}  // namespace

void setup()
{
    application.begin();
}

void loop()
{
    application.update();
}

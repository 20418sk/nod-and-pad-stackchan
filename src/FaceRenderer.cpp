/*
 * 製品版StackChan公式ファームのdefault skinをM5GFX向けに移植。
 * Original: Copyright (c) 2026 M5Stack Technology CO LTD, MIT License
 * Source revision: b72b3ede38b32d54f0b6ba51c62cfcef2ec3ae1e
 */
#include "FaceRenderer.h"

#include "AppConfig.h"

#include <cstdio>

namespace {
constexpr uint16_t kBackground = TFT_BLACK;
constexpr uint16_t kPrimary    = TFT_WHITE;
constexpr uint16_t kHeart      = 0xE986;  // 公式デコレータの #E13232 に近いRGB565
constexpr uint16_t kBlush      = 0xFD33;  // 公式デコレータの #F7A59E に近いRGB565
constexpr uint16_t kPanel      = 0x0841;
constexpr uint16_t kDebugText  = 0xBDF7;

// 公式default skin: 320x240、目は中心から左右70px・上16px、口は下26px。
constexpr int kCenterX  = 160;
constexpr int kCenterY  = 120;
constexpr int kLeftEyeX = kCenterX - 70;
constexpr int kRightEyeX = kCenterX + 70;
constexpr int kEyeY      = kCenterY - 16;
constexpr int kMouthY    = kCenterY + 26;

bool timeElapsed(uint32_t nowMs, uint32_t sinceMs, uint32_t durationMs)
{
    return static_cast<uint32_t>(nowMs - sinceMs) >= durationMs;
}
}  // namespace

bool FaceRenderer::begin(LGFX_Device& display)
{
    display_ = &display;
    display_->setRotation(1);
    display_->setTextWrap(false);
    display_->setTextDatum(middle_center);
    display_->setTextColor(kPrimary, kBackground);

    // 顔はPSRAM上の画面外バッファへ完成形を描いてから一括転送する。
    // fillScreen() と各パーツの描画途中がLCDに見えることを防ぐ。
    canvas_.setPsram(true);
    canvas_.setColorDepth(16);
    canvasReady_ = canvas_.createSprite(display_->width(), display_->height()) !=
                   nullptr;
    drawTarget_ = canvasReady_ ? static_cast<lgfx::LovyanGFX*>(&canvas_)
                               : static_cast<lgfx::LovyanGFX*>(display_);
    drawTarget_->setTextWrap(false);
    drawTarget_->setTextDatum(middle_center);
    drawTarget_->setTextColor(kPrimary, kBackground);
    return display_->width() > 0 && display_->height() > 0;
}

void FaceRenderer::setDebugEnabled(bool enabled)
{
    if (debugEnabled_ != enabled) {
        debugEnabled_ = enabled;
        hasDrawnFace_ = false;
    }
}

void FaceRenderer::setPettingTapStyle(bool enabled)
{
    pettingTapStyle_ = enabled;
    hasDrawnFace_ = false;
}

void FaceRenderer::startNodAfterglow(uint32_t nowMs)
{
    nodAfterglowActive_ = true;
    nodAfterglowStartedMs_ = nowMs;
    nodAfterglowStage_ = 1;
    hasDrawnFace_ = false;
}

void FaceRenderer::render(FaceExpression expression,
                          const FaceDebugInfo& debugInfo, uint32_t nowMs,
                          bool force)
{
    if (display_ == nullptr || overlayActive_) {
        return;
    }

    const bool expressionChanged = expression != lastExpression_;
    if (expressionChanged && lastExpression_ == FaceExpression::PETTING &&
        expression != FaceExpression::PETTING) {
        pettingFadeActive_ = true;
        pettingFadeStartedMs_ = nowMs;
        pettingFadeStage_ = 1;
    } else if (expressionChanged && expression == FaceExpression::PETTING) {
        pettingFadeActive_ = false;
        pettingFadeStage_ = 0;
    } else if (expressionChanged && expression != FaceExpression::PETTING) {
        pettingTapStyle_ = false;
    }
    if (expressionChanged && lastExpression_ == FaceExpression::SLEEPING &&
        expression == FaceExpression::NORMAL) {
        wakeAnimationActive_ = true;
        wakeAnimationStartedMs_ = nowMs;
        wakeTransitionStage_ = 1;
    } else if (expressionChanged && expression != FaceExpression::NORMAL) {
        wakeAnimationActive_ = false;
        wakeTransitionStage_ = 0;
    }
    const bool animationChanged = updateAnimationState(
        expression, nowMs, force || !hasDrawnFace_ || expressionChanged);

    if (force || !hasDrawnFace_ || expressionChanged || animationChanged) {
        drawFace(expression);
        lastExpression_ = expression;
        hasDrawnFace_    = true;
        lastDebugDrawMs_ = nowMs;
        if (debugEnabled_) {
            drawDebug(debugInfo);
        }
        present();
        return;
    }

    if (debugEnabled_ &&
        timeElapsed(nowMs, lastDebugDrawMs_,
                    app_config::display::kDebugRefreshMs)) {
        drawDebug(debugInfo);
        lastDebugDrawMs_ = nowMs;
        present();
    }
}

bool FaceRenderer::supportsIdleAnimation(FaceExpression expression)
{
    return expression == FaceExpression::NORMAL ||
           expression == FaceExpression::LISTENING ||
           expression == FaceExpression::PETTING ||
           expression == FaceExpression::SLEEPING;
}

bool FaceRenderer::updateAnimationState(FaceExpression expression,
                                        uint32_t nowMs, bool reset)
{
    const int8_t previousOffsetY = faceOffsetY_;
    const int8_t previousGazeX = gazeOffsetX_;
    const bool previousBlink = blinkClosed_;
    const bool previousPulse = listeningPulse_;
    const uint8_t previousFadeStage = pettingFadeStage_;
    const uint8_t previousSleepStage = sleepTransitionStage_;
    const uint8_t previousWakeStage = wakeTransitionStage_;
    const uint8_t previousHeartPhase = pettingHeartPhase_;
    const uint8_t previousHeartRadius = pettingHeartRadius_;
    const uint8_t previousAfterglowStage = nodAfterglowStage_;
    const uint8_t previousSleepIndicatorCount = sleepIndicatorCount_;

    if (reset) {
        expressionStartedMs_ = nowMs;
    }

    faceOffsetY_ = 0;
    gazeOffsetX_ = 0;
    blinkClosed_ = false;
    listeningPulse_ = false;
    sleepTransitionStage_ = 0;
    pettingHeartPhase_ = 0;
    pettingHeartRadius_ = 7;
    sleepIndicatorCount_ = 1;

    if (pettingFadeActive_) {
        const uint32_t fadeAge =
            static_cast<uint32_t>(nowMs - pettingFadeStartedMs_);
        if (fadeAge >= app_config::display::kPettingDecorationFadeMs) {
            pettingFadeActive_ = false;
            pettingFadeStage_ = 0;
        } else {
            pettingFadeStage_ = static_cast<uint8_t>(
                1U + (fadeAge * 3U) /
                         app_config::display::kPettingDecorationFadeMs);
        }
    }

    if (expression == FaceExpression::SLEEPING) {
        const uint32_t sleepAge =
            static_cast<uint32_t>(nowMs - expressionStartedMs_);
        sleepTransitionStage_ = sleepAge >= app_config::display::kSleepFaceTransitionMs
            ? 3U
            : static_cast<uint8_t>(
                  1U + (sleepAge * 3U) /
                           app_config::display::kSleepFaceTransitionMs);
        if (sleepAge >= app_config::display::kSleepFaceTransitionMs) {
            constexpr uint8_t counts[3] = {1, 2, 3};
            const uint32_t settledAge =
                sleepAge - app_config::display::kSleepFaceTransitionMs;
            sleepIndicatorCount_ = counts[
                (settledAge / app_config::display::kSleepIndicatorStepMs) %
                3U];
        }
    }

    if (expression == FaceExpression::PETTING) {
        const uint32_t pettingAge =
            static_cast<uint32_t>(nowMs - expressionStartedMs_);
        pettingHeartPhase_ = static_cast<uint8_t>(
            (pettingAge / app_config::display::kPettingHeartStepMs) % 4U);
        if (pettingTapStyle_ &&
            pettingAge < app_config::display::kPettingTapPopDurationMs) {
            constexpr uint8_t popRadii[4] = {6, 9, 8, 7};
            const uint8_t popStage = static_cast<uint8_t>(
                pettingAge / app_config::display::kPettingTapPopStepMs);
            pettingHeartRadius_ = popRadii[popStage > 3U ? 3U : popStage];
        }
    }

    if (nodAfterglowActive_) {
        const uint32_t afterglowAge =
            static_cast<uint32_t>(nowMs - nodAfterglowStartedMs_);
        if (expression != FaceExpression::NORMAL ||
            afterglowAge >= app_config::display::kNodAfterglowDurationMs) {
            nodAfterglowActive_ = false;
            nodAfterglowStage_ = 0;
        } else if (afterglowAge < 360U) {
            nodAfterglowStage_ = 1;
        } else if (afterglowAge < 540U) {
            nodAfterglowStage_ = 2;
        } else {
            nodAfterglowStage_ = 3;
        }
    }

    if (wakeAnimationActive_) {
        const uint32_t wakeAge =
            static_cast<uint32_t>(nowMs - wakeAnimationStartedMs_);
        if (wakeAge >= app_config::display::kWakeFaceTransitionMs) {
            wakeAnimationActive_ = false;
            wakeTransitionStage_ = 0;
        } else {
            wakeTransitionStage_ = static_cast<uint8_t>(
                1U + (wakeAge * 3U) /
                         app_config::display::kWakeFaceTransitionMs);
        }
    }

    // 画面外バッファがない場合は、部分描画が見えてしまうため静止画を維持する。
    if (!canvasReady_ ||
        (!supportsIdleAnimation(expression) && !pettingFadeActive_ &&
         previousFadeStage == pettingFadeStage_)) {
        return previousOffsetY != faceOffsetY_ ||
               previousGazeX != gazeOffsetX_ ||
               previousBlink != blinkClosed_ ||
               previousPulse != listeningPulse_ ||
               previousFadeStage != pettingFadeStage_ ||
               previousSleepStage != sleepTransitionStage_ ||
               previousWakeStage != wakeTransitionStage_ ||
               previousHeartPhase != pettingHeartPhase_ ||
               previousHeartRadius != pettingHeartRadius_ ||
               previousAfterglowStage != nodAfterglowStage_ ||
               previousSleepIndicatorCount != sleepIndicatorCount_;
    }

    const uint32_t age = static_cast<uint32_t>(nowMs - expressionStartedMs_);
    const uint32_t breathingPhase =
        (age % app_config::display::kBreathingPeriodMs) /
        (app_config::display::kBreathingPeriodMs / 8U);
    constexpr int8_t breathingOffsets[8] = {0, -1, -1, 0, 0, 1, 1, 0};
    faceOffsetY_ = breathingOffsets[breathingPhase];

    if (expression == FaceExpression::NORMAL ||
        expression == FaceExpression::LISTENING) {
        const uint32_t blinkPhase = age % app_config::display::kBlinkPeriodMs;
        const bool doubleBlinkCycle =
            ((age / app_config::display::kBlinkPeriodMs) % 4U) == 3U;
        const bool firstDoubleBlink =
            doubleBlinkCycle &&
            blinkPhase >= (app_config::display::kBlinkPeriodMs - 420U) &&
            blinkPhase < (app_config::display::kBlinkPeriodMs - 280U);
        const bool regularBlink =
            blinkPhase >= (app_config::display::kBlinkPeriodMs -
                           app_config::display::kBlinkDurationMs);
        blinkClosed_ = firstDoubleBlink || regularBlink;

        constexpr int8_t gazeOffsets[5] = {0, 1, 0, -1, 0};
        gazeOffsetX_ = gazeOffsets[
            (age / app_config::display::kGazeStepMs) % 5U];
    }

    if (expression == FaceExpression::LISTENING) {
        listeningPulse_ =
            ((age / app_config::display::kListeningPulseMs) % 2U) != 0U;
    }

    return previousOffsetY != faceOffsetY_ ||
           previousGazeX != gazeOffsetX_ ||
           previousBlink != blinkClosed_ ||
           previousPulse != listeningPulse_ ||
           previousFadeStage != pettingFadeStage_ ||
           previousSleepStage != sleepTransitionStage_ ||
           previousWakeStage != wakeTransitionStage_ ||
           previousHeartPhase != pettingHeartPhase_ ||
           previousHeartRadius != pettingHeartRadius_ ||
           previousAfterglowStage != nodAfterglowStage_ ||
           previousSleepIndicatorCount != sleepIndicatorCount_;
}

void FaceRenderer::drawFace(FaceExpression expression)
{
    drawTarget_->startWrite();
    drawTarget_->fillScreen(kBackground);

    switch (expression) {
        case FaceExpression::NORMAL:
            if (nodAfterglowActive_ && nodAfterglowStage_ == 1) {
                drawHappyEye(kLeftEyeX, kEyeY + faceOffsetY_, true);
                drawHappyEye(kRightEyeX, kEyeY + faceOffsetY_, false);
            } else if (nodAfterglowActive_ && nodAfterglowStage_ == 2) {
                drawSleepyEye(kLeftEyeX, kEyeY + faceOffsetY_);
                drawSleepyEye(kRightEyeX, kEyeY + faceOffsetY_);
            } else if (wakeAnimationActive_ && wakeTransitionStage_ == 1) {
                drawSleepyEye(kLeftEyeX, kEyeY + faceOffsetY_);
                drawSleepyEye(kRightEyeX, kEyeY + faceOffsetY_);
            } else if (wakeAnimationActive_ && wakeTransitionStage_ == 2) {
                drawRoundEye(kLeftEyeX, kEyeY + faceOffsetY_, 6);
                drawRoundEye(kRightEyeX, kEyeY + faceOffsetY_, 6);
            } else if (blinkClosed_) {
                drawSleepyEye(kLeftEyeX, kEyeY + faceOffsetY_);
                drawSleepyEye(kRightEyeX, kEyeY + faceOffsetY_);
            } else {
                drawRoundEye(kLeftEyeX + gazeOffsetX_,
                             kEyeY + faceOffsetY_, 10);
                drawRoundEye(kRightEyeX + gazeOffsetX_,
                             kEyeY + faceOffsetY_, 10);
            }
            drawOfficialMouth(0);
            break;

        case FaceExpression::LISTENING:
            // 公式setSizeの拡大表現を傾聴用に控えめに適用。
            if (blinkClosed_) {
                drawSleepyEye(kLeftEyeX, kEyeY + faceOffsetY_);
                drawSleepyEye(kRightEyeX, kEyeY + faceOffsetY_);
            } else {
                const int radius = listeningPulse_ ? 14 : 13;
                drawRoundEye(kLeftEyeX + gazeOffsetX_,
                             kEyeY + faceOffsetY_, radius);
                drawRoundEye(kRightEyeX + gazeOffsetX_,
                             kEyeY + faceOffsetY_, radius);
            }
            drawOfficialMouth(listeningPulse_ ? 22 : 18);
            break;

        case FaceExpression::NODDING:
            // 公式Happy: eyelid weight=72、左右rotation=±1550。
            drawHappyEye(kLeftEyeX, kEyeY + faceOffsetY_, true);
            drawHappyEye(kRightEyeX, kEyeY + faceOffsetY_, false);
            drawOfficialMouth(0);
            break;

        case FaceExpression::PETTING:
            drawHappyEye(kLeftEyeX, kEyeY + faceOffsetY_, true);
            drawHappyEye(kRightEyeX, kEyeY + faceOffsetY_, false);
            drawOfficialMouth(0);
            // 公式HeadPetModifierと同じ位置・色系統のハートと照れ頬。
            {
                constexpr int8_t heartOffsets[4] = {0, -3, 0, 2};
                const int heartY = heartOffsets[pettingHeartPhase_];
                drawHeart(kCenterX + 108,
                          kCenterY - 70 + faceOffsetY_ + heartY,
                          pettingHeartRadius_);
            }
            drawBlush(kCenterX - 108, kCenterY + 28 + faceOffsetY_);
            drawBlush(kCenterX + 108, kCenterY + 28 + faceOffsetY_);
            break;

        case FaceExpression::SLEEPING:
            // 公式Sleepy: eyelid weight=35。
            if (sleepTransitionStage_ == 1) {
                drawRoundEye(kLeftEyeX, kEyeY + faceOffsetY_, 8);
                drawRoundEye(kRightEyeX, kEyeY + faceOffsetY_, 8);
                drawOfficialMouth(4);
            } else {
                drawSleepyEye(kLeftEyeX, kEyeY + faceOffsetY_);
                drawSleepyEye(kRightEyeX, kEyeY + faceOffsetY_);
                drawOfficialMouth(sleepTransitionStage_ == 2 ? 5 : 8);
            }
            if (sleepTransitionStage_ >= 2) {
                drawSleepIndicator(sleepIndicatorCount_);
            }
            break;

        case FaceExpression::ERROR:
            // 公式Doubtを土台にした困り顔。
            drawThickLine(kLeftEyeX - 9, kEyeY - 2, kLeftEyeX + 9,
                          kEyeY + 3, 4, kPrimary);
            drawThickLine(kRightEyeX - 9, kEyeY + 3,
                          kRightEyeX + 9, kEyeY - 2, 4, kPrimary);
            drawThickLine(kCenterX - 25, kMouthY + 4, kCenterX,
                          kMouthY - 3, 4, kPrimary);
            drawThickLine(kCenterX, kMouthY - 3, kCenterX + 25,
                          kMouthY + 4, 4, kPrimary);
            break;
    }

    if (pettingFadeActive_) {
        if (pettingFadeStage_ == 1) {
            drawHeart(kCenterX + 108, kCenterY - 70 + faceOffsetY_);
            drawBlush(kCenterX - 108, kCenterY + 28 + faceOffsetY_);
            drawBlush(kCenterX + 108, kCenterY + 28 + faceOffsetY_);
        } else if (pettingFadeStage_ == 2) {
            drawSmallHeart(kCenterX + 108, kCenterY - 77 + faceOffsetY_);
            drawBlush(kCenterX - 108, kCenterY + 28 + faceOffsetY_);
            drawBlush(kCenterX + 108, kCenterY + 28 + faceOffsetY_);
        } else {
            drawSmallHeart(kCenterX + 108, kCenterY - 84 + faceOffsetY_);
        }
    }

    drawTarget_->endWrite();
}

void FaceRenderer::present()
{
    if (canvasReady_) {
        canvas_.pushSprite(display_, 0, 0);
    }
}

void FaceRenderer::drawRoundEye(int x, int y, int radius)
{
    drawTarget_->fillCircle(x, y, radius, kPrimary);
}

void FaceRenderer::drawSleepIndicator(uint8_t stage)
{
    const int y = faceOffsetY_;
    const auto drawZ = [this](int x, int top, int width, int height,
                              int thickness) {
        drawThickLine(x, top, x + width, top, thickness, kPrimary);
        drawThickLine(x + width, top, x, top + height, thickness, kPrimary);
        drawThickLine(x, top + height, x + width, top + height, thickness,
                      kPrimary);
    };

    // 内蔵の小フォントは実機で潰れやすいため、線で明瞭な睡眠マークを描く。
    drawZ(260, 45 + y, 22, 18, 3);
    if (stage >= 2) {
        drawZ(286, 29 + y, 15, 12, 2);
    }
    if (stage >= 3) {
        drawZ(305, 17 + y, 9, 8, 1);
    }
}

void FaceRenderer::drawHappyEye(int x, int y, bool left)
{
    if (left) {
        drawThickLine(x - 10, y + 5, x + 9, y - 5, 6, kPrimary);
    } else {
        drawThickLine(x - 9, y - 5, x + 10, y + 5, 6, kPrimary);
    }
}

void FaceRenderer::drawSleepyEye(int x, int y)
{
    drawThickLine(x - 10, y + 1, x + 10, y, 4, kPrimary);
}

void FaceRenderer::drawOfficialMouth(int weight)
{
    if (weight < 0) {
        weight = 0;
    } else if (weight > 100) {
        weight = 100;
    }

    // 公式値: weight 0で90x6、100で60x50、radius 0..16。
    const int width  = 90 - ((30 * weight) / 100);
    const int height = 6 + ((44 * weight) / 100);
    const int radius = (16 * weight) / 100;
    drawTarget_->fillRoundRect(kCenterX - (width / 2),
                               kMouthY + faceOffsetY_ - (height / 2),
                               width, height, radius,
                               kPrimary);
}

void FaceRenderer::drawHeart(int x, int y, int radius)
{
    drawTarget_->fillCircle(x - radius, y - 3, radius, kHeart);
    drawTarget_->fillCircle(x + radius, y - 3, radius, kHeart);
    drawTarget_->fillTriangle(x - (radius * 2), y, x + (radius * 2), y,
                              x, y + (radius * 2) + 4, kHeart);
}

void FaceRenderer::drawSmallHeart(int x, int y)
{
    constexpr int radius = 4;
    drawTarget_->fillCircle(x - radius, y - 2, radius, kHeart);
    drawTarget_->fillCircle(x + radius, y - 2, radius, kHeart);
    drawTarget_->fillTriangle(x - 8, y, x + 8, y, x, y + 11, kHeart);
}

void FaceRenderer::drawBlush(int x, int y)
{
    for (int offset = -8; offset <= 8; offset += 8) {
        drawThickLine(x + offset - 4, y + 5, x + offset + 4, y - 5, 2,
                      kBlush);
    }
}

void FaceRenderer::drawThickLine(int x0, int y0, int x1, int y1,
                                 int thickness, uint16_t color)
{
    const int half = thickness / 2;
    for (int offset = -half; offset <= half; ++offset) {
        drawTarget_->drawLine(x0, y0 + offset, x1, y1 + offset, color);
    }
}

void FaceRenderer::drawDebug(const FaceDebugInfo& info)
{
    char line[64];
    constexpr int panelHeight = 70;
    const int panelY = drawTarget_->height() - panelHeight;
    drawTarget_->startWrite();
    drawTarget_->fillRect(0, panelY, drawTarget_->width(), panelHeight, kPanel);
    drawTarget_->setFont(&fonts::Font0);
    drawTarget_->setTextDatum(top_left);
    drawTarget_->setTextColor(kDebugText, kPanel);

    std::snprintf(line, sizeof(line), "ST:%-11s LAST:%lums", info.stateName,
                  static_cast<unsigned long>(info.lastSpeechMs));
    drawTarget_->drawString(line, 4, panelY + 4);
    std::snprintf(line, sizeof(line), "L:%6.0f R:%6.0f DIR:%s LAG:%+d",
                  static_cast<double>(info.leftLevel),
                  static_cast<double>(info.rightLevel), info.directionName,
                  info.directionLagSamples);
    drawTarget_->drawString(line, 4, panelY + 21);
    std::snprintf(line, sizeof(line), "LEVEL:%7.1f  NOISE:%7.1f",
                  static_cast<double>(info.smoothedLevel),
                  static_cast<double>(info.noiseFloor));
    drawTarget_->drawString(line, 4, panelY + 38);
    std::snprintf(line, sizeof(line), "TH:%7.1f CONF:%4.2f YAW:%+d",
                  static_cast<double>(info.dynamicThreshold),
                  static_cast<double>(info.directionConfidence),
                  info.yawTarget);
    drawTarget_->drawString(line, 4, panelY + 55);
    drawTarget_->endWrite();
}

void FaceRenderer::showStartupCheck()
{
    if (display_ == nullptr) {
        return;
    }
    overlayActive_ = true;
    display_->startWrite();
    display_->fillScreen(kBackground);
    display_->setTextDatum(middle_center);
    display_->setTextSize(1.0f);
    display_->setFont(&fonts::Font4);
    display_->setTextColor(kPrimary, kBackground);
    display_->drawString("SERVO TESTING", display_->width() / 2,
                         display_->height() / 2 - 28);
    display_->setTextFont(3);
    display_->setTextSize(2.0f);
    display_->setTextColor(kBlush, kBackground);
    display_->drawString("PLEASE DO NOT TOUCH", display_->width() / 2,
                         display_->height() / 2 + 30);
    display_->endWrite();
}

void FaceRenderer::showCalibration(bool startup, uint8_t secondsRemaining)
{
    if (display_ == nullptr) {
        return;
    }
    overlayActive_ = true;
    display_->startWrite();
    display_->fillScreen(kBackground);
    display_->setTextDatum(middle_center);
    display_->setTextSize(1.0f);
    display_->setFont(&fonts::Font4);
    display_->setTextColor(kPrimary, kBackground);
    display_->drawString(startup ? "MIC CALIBRATION"
                                 : "MIC RECALIBRATION",
                         display_->width() / 2,
                         display_->height() / 2 - 28);
    // Use the requested Font3 slot for the supplementary countdown text.
    display_->setTextFont(3);
    display_->setTextSize(2.0f);
    display_->setTextColor(kBlush, kBackground);
    display_->drawString("PLEASE BE QUIET", display_->width() / 2,
                         display_->height() / 2 + 12);
    char countdown[32]{};
    if (secondsRemaining >= 3) {
        std::snprintf(countdown, sizeof(countdown), "3..");
    } else if (secondsRemaining == 2) {
        std::snprintf(countdown, sizeof(countdown), "3..2..");
    } else {
        std::snprintf(countdown, sizeof(countdown), "3..2..1..");
    }
    display_->drawString(countdown,
                         display_->width() / 2,
                         display_->height() / 2 + 52);
    display_->endWrite();
}

void FaceRenderer::showStartupGuide(uint8_t page)
{
    if (display_ == nullptr) {
        return;
    }
    overlayActive_ = true;
    display_->startWrite();
    display_->fillScreen(kBackground);
    display_->setTextDatum(middle_center);
    display_->setTextSize(1.0f);
    const char* title = "SERVO TEST COMPLETE!";
    const char* detail = "TAP TO TEST MIC";
    uint16_t detailColor = TFT_GREEN;
    if (page == 2) {
        title = "ALL TESTS COMPLETE!";
        detail = "TAP TO CONTINUE";
    } else if (page == 3) {
        title = "YOUR PRIVACY";
        detail = "TAP TO CONTINUE";
        detailColor = TFT_GREEN;
    } else if (page >= 4) {
        title = "READY TO LISTEN";
        detail = "TAP TO START";
    }
    display_->setFont(&fonts::Font4);
    display_->setTextColor(kPrimary, kBackground);
    if (page == 3) {
        display_->drawString(title, display_->width() / 2,
                             display_->height() / 2 - 88);
        // M5GFX's Font3 slot maps to its compact built-in font.
        display_->setTextFont(3);
        display_->setTextSize(2.0f);
        display_->drawString("NO CAMERA", display_->width() / 2,
                             display_->height() / 2 - 52);
        display_->drawString("NO RECORDING", display_->width() / 2,
                             display_->height() / 2 - 20);
        display_->drawString("NO SPEECH ANALYSIS", display_->width() / 2,
                             display_->height() / 2 + 12);
        display_->drawString("LOCAL PROCESSING ONLY", display_->width() / 2,
                             display_->height() / 2 + 44);
        display_->setTextSize(1.0f);
    } else {
        display_->drawString(title, display_->width() / 2,
                             display_->height() / 2 - 28);
    }
    display_->setTextFont(3);
    display_->setTextSize(2.0f);
    display_->setTextColor(detailColor, kBackground);
    display_->drawString(detail, display_->width() / 2,
                         display_->height() / 2 + (page == 3 ? 90 : 30));
    display_->endWrite();
}

void FaceRenderer::showError(const char* shortMessage)
{
    if (display_ == nullptr) {
        return;
    }
    overlayActive_ = false;
    drawFace(FaceExpression::ERROR);
    drawTarget_->startWrite();
    drawTarget_->fillRect(0, drawTarget_->height() - 44,
                          drawTarget_->width(), 44, kBackground);
    drawTarget_->setFont(&fonts::efontJA_16);
    drawTarget_->setTextDatum(middle_center);
    drawTarget_->setTextColor(TFT_RED, kBackground);
    drawTarget_->drawString(shortMessage, drawTarget_->width() / 2,
                            drawTarget_->height() - 22);
    drawTarget_->endWrite();
    present();
    lastExpression_ = FaceExpression::ERROR;
    hasDrawnFace_   = true;
}

void FaceRenderer::clearOverlay()
{
    overlayActive_ = false;
    hasDrawnFace_  = false;
}

#include "CameraDiagnostics.h"

#include "AppConfig.h"
#include "CameraFrameAnalyzer.h"
#include "FaceCandidateEstimator.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <driver/i2c.h>
#include <esp_camera.h>

#include <algorithm>

bool CameraDiagnostics::begin()
{
    if (!psramFound()) {
        portENTER_CRITICAL(&infoMux_);
        info_.initError = ESP_ERR_NO_MEM;
        portEXIT_CRITICAL(&infoMux_);
        return false;
    }

    camera_config_t config{};
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = -1;
    // M5UnifiedがCoreS3内部I2Cをport 1で初期化済み。公式ピン12/11の
    // バスを共有し、release/re-initによるマイクやタッチへの影響を避ける。
    config.pin_sccb_sda = -1;
    config.pin_sccb_scl = -1;
    config.pin_d7 = 47;
    config.pin_d6 = 48;
    config.pin_d5 = 16;
    config.pin_d4 = 15;
    config.pin_d3 = 42;
    config.pin_d2 = 41;
    config.pin_d1 = 40;
    config.pin_d0 = 39;
    config.pin_vsync = 46;
    config.pin_href = 38;
    config.pin_pclk = 45;
    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;
    // 診断段階は画素のバイト順に依存しない直接輝度を使う。
    // RGB565より転送量も少なく、マイク処理への負荷を抑えられる。
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 0;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.sccb_i2c_port = I2C_NUM_1;

    const esp_err_t error = esp_camera_init(&config);
    if (error != ESP_OK || esp_camera_sensor_get() == nullptr) {
        portENTER_CRITICAL(&infoMux_);
        info_.initError = static_cast<int>(error);
        portEXIT_CRITICAL(&infoMux_);
        return false;
    }

    const BaseType_t taskResult = xTaskCreatePinnedToCore(
        taskEntry, "camera_diag", app_config::camera::kTaskStackWords, this,
        app_config::camera::kTaskPriority, &taskHandle_,
        app_config::camera::kTaskCore);
    if (taskResult != pdPASS) {
        (void)esp_camera_deinit();
        portENTER_CRITICAL(&infoMux_);
        info_.initError = ESP_ERR_NO_MEM;
        portEXIT_CRITICAL(&infoMux_);
        taskHandle_ = nullptr;
        return false;
    }

    portENTER_CRITICAL(&infoMux_);
    info_.initialized = true;
    info_.healthy = true;
    info_.initError = ESP_OK;
    portEXIT_CRITICAL(&infoMux_);
    return true;
}

bool CameraDiagnostics::requestCapture(uint32_t nowMs)
{
    if (taskHandle_ == nullptr || requestPending_.load() ||
        (hasRequested_ && static_cast<uint32_t>(nowMs - lastRequestMs_) <
                              app_config::camera::kMinimumCaptureIntervalMs)) {
        return false;
    }
    bool expected = false;
    if (!requestPending_.compare_exchange_strong(expected, true)) {
        return false;
    }
    hasRequested_ = true;
    lastRequestMs_ = nowMs;
    portENTER_CRITICAL(&infoMux_);
    info_.capturePending = true;
    portEXIT_CRITICAL(&infoMux_);
    xTaskNotifyGive(taskHandle_);
    return true;
}

CameraDiagnosticInfo CameraDiagnostics::info() const
{
    portENTER_CRITICAL(&infoMux_);
    const CameraDiagnosticInfo copy = info_;
    portEXIT_CRITICAL(&infoMux_);
    return copy;
}

void CameraDiagnostics::taskEntry(void* argument)
{
    static_cast<CameraDiagnostics*>(argument)->taskLoop();
}

void CameraDiagnostics::taskLoop()
{
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t startedMs = millis();

        // GRAB_WHEN_EMPTY + one framebuffer leaves a frame captured just after
        // the previous return waiting in the queue. Discard it so this request
        // measures the scene at speech time, then wait for one fresh frame.
        camera_fb_t* staleFrame = esp_camera_fb_get();
        if (staleFrame == nullptr) {
            recordFailure(static_cast<uint32_t>(millis() - startedMs));
            requestPending_.store(false);
            continue;
        }
        esp_camera_fb_return(staleFrame);

        camera_fb_t* frame = esp_camera_fb_get();
        const uint32_t captureMs = static_cast<uint32_t>(millis() - startedMs);
        if (frame == nullptr || frame->format != PIXFORMAT_GRAYSCALE) {
            if (frame != nullptr) {
                esp_camera_fb_return(frame);
            }
            recordFailure(captureMs);
            requestPending_.store(false);
            continue;
        }

        const CameraFrameStats stats = analyzeGrayscale(
            frame->buf, frame->len, frame->width, frame->height,
            app_config::camera::kAnalysisStridePixels);
        const FaceCandidateEstimate face = estimateFaceCandidate(
            frame->buf, frame->len, frame->width, frame->height);
        const uint16_t width = static_cast<uint16_t>(
            std::min<std::size_t>(frame->width, UINT16_MAX));
        const uint16_t height = static_cast<uint16_t>(
            std::min<std::size_t>(frame->height, UINT16_MAX));
        esp_camera_fb_return(frame);

        if (!stats.valid) {
            recordFailure(captureMs);
            requestPending_.store(false);
            continue;
        }

        portENTER_CRITICAL(&infoMux_);
        info_.healthy = true;
        info_.frameReady = true;
        info_.width = width;
        info_.height = height;
        info_.meanLuma = stats.meanLuma;
        info_.contrastRange = stats.contrastRange;
        info_.captureMs = captureMs;
        info_.faceCandidate = face;
        ++info_.captureCount;
        info_.capturePending = false;
        portEXIT_CRITICAL(&infoMux_);
        requestPending_.store(false);
    }
}

void CameraDiagnostics::recordFailure(uint32_t captureMs)
{
    portENTER_CRITICAL(&infoMux_);
    info_.captureMs = captureMs;
    ++info_.failureCount;
    info_.capturePending = false;
    info_.healthy = info_.failureCount <
                    app_config::camera::kFailureWarningCount;
    portEXIT_CRITICAL(&infoMux_);
}

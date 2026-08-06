#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "human_face_detect.hpp"

#include <algorithm>
#include <cstdint>
#include <new>

namespace {

constexpr char kTag[] = "face_diag";
constexpr TickType_t kCaptureInterval = pdMS_TO_TICKS(900);

struct FrameStats {
    uint8_t meanLuma;
    uint8_t lumaRange;
};

FrameStats sampleYuv422(const camera_fb_t& frame, bool yuyv)
{
    uint32_t sum = 0;
    uint32_t count = 0;
    uint8_t minimum = 255;
    uint8_t maximum = 0;
    constexpr size_t kByteStride = 32;

    // YUYV has luma at even bytes; UYVY has luma at odd bytes.
    for (size_t byteIndex = yuyv ? 0U : 1U; byteIndex < frame.len;
         byteIndex += kByteStride) {
        const uint8_t luma = frame.buf[byteIndex];
        sum += luma;
        ++count;
        minimum = std::min(minimum, luma);
        maximum = std::max(maximum, luma);
    }

    if (count == 0U) {
        return {0, 0};
    }
    return {
        static_cast<uint8_t>(sum / count),
        static_cast<uint8_t>(maximum - minimum),
    };
}

camera_config_t makeCameraConfig()
{
    camera_config_t config{};
    config.pin_pwdn = -1;
    config.pin_reset = -1;
    config.pin_xclk = -1;
    config.pin_sccb_sda = 12;
    config.pin_sccb_scl = 11;
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
    config.pixel_format = PIXFORMAT_YUV422;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 0;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.sccb_i2c_port = -1;
    return config;
}

const char* rawDirection(int offsetPercent)
{
    if (offsetPercent < -10) {
        return "L";
    }
    if (offsetPercent > 10) {
        return "R";
    }
    return "C";
}

void runDiagnostic()
{
    ESP_LOGI(kTag, "StackChan K151 ESP-DL face diagnostic");
    ESP_LOGW(kTag, "serial only / no Wi-Fi / no storage / no servo commands");
    ESP_LOGI(kTag, "PSRAM free before init: %u bytes",
             static_cast<unsigned int>(
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    camera_config_t cameraConfig = makeCameraConfig();
    const esp_err_t cameraError = esp_camera_init(&cameraConfig);
    if (cameraError != ESP_OK) {
        ESP_LOGE(kTag, "GC0308 init failed: 0x%x",
                 static_cast<unsigned int>(cameraError));
        return;
    }

    HumanFaceDetect* detector = new (std::nothrow) HumanFaceDetect(
        HumanFaceDetect::ESPDET_PICO_224_224_FACE, false);
    if (detector == nullptr) {
        ESP_LOGE(kTag, "HumanFaceDetect allocation failed");
        esp_camera_deinit();
        return;
    }
    ESP_LOGI(kTag, "camera and official ESPDet Pico 224 model initialized");
    ESP_LOGI(kTag, "PSRAM free after init: %u bytes",
             static_cast<unsigned int>(
                 heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));

    uint32_t frameNumber = 0;
    for (;;) {
        camera_fb_t* frame = esp_camera_fb_get();
        if (frame == nullptr || frame->format != PIXFORMAT_YUV422) {
            ESP_LOGW(kTag, "capture failed");
            if (frame != nullptr) {
                esp_camera_fb_return(frame);
            }
            vTaskDelay(kCaptureInterval);
            continue;
        }

        constexpr float scoreThreshold = 0.50F;
        const FrameStats stats = sampleYuv422(*frame, true);
        dl::image::img_t image{};
        image.data = frame->buf;
        image.width = static_cast<uint16_t>(frame->width);
        image.height = static_cast<uint16_t>(frame->height);
        image.pix_type = dl::image::DL_IMAGE_PIX_TYPE_YUYV;
        detector->set_score_thr(scoreThreshold, 0);

        const int64_t inferenceStartedUs = esp_timer_get_time();
        auto& results = detector->run(image);
        const uint32_t inferenceMs = static_cast<uint32_t>(
            (esp_timer_get_time() - inferenceStartedUs) / 1000);
        ++frameNumber;

        const dl::detect::result_t* best = nullptr;
        for (const auto& result : results) {
            if (result.box.size() < 4U) {
                continue;
            }
            if (best == nullptr || result.score > best->score) {
                best = &result;
            }
        }

        if (best == nullptr) {
            ESP_LOGI(kTag,
                     "#%lu fmt=%s thr=%.2f Y=%u C=%u "
                     "FACE:none count=%u infer=%lums",
                     static_cast<unsigned long>(frameNumber),
                     "YUYV",
                     static_cast<double>(scoreThreshold),
                     static_cast<unsigned int>(stats.meanLuma),
                     static_cast<unsigned int>(stats.lumaRange),
                     static_cast<unsigned int>(results.size()),
                     static_cast<unsigned long>(inferenceMs));
        } else {
            const int centerX = (best->box[0] + best->box[2]) / 2;
            const int imageCenter = static_cast<int>(frame->width / 2U);
            const int offsetPercent = std::clamp(
                (centerX - imageCenter) * 100 / imageCenter, -100, 100);
            ESP_LOGI(kTag,
                     "#%lu fmt=%s thr=%.2f Y=%u C=%u "
                     "FACE:%s score=%.3f x=%+d%% "
                     "box=[%d,%d,%d,%d] count=%u infer=%lums",
                     static_cast<unsigned long>(frameNumber),
                     "YUYV",
                     static_cast<double>(scoreThreshold),
                     static_cast<unsigned int>(stats.meanLuma),
                     static_cast<unsigned int>(stats.lumaRange),
                     rawDirection(offsetPercent),
                     static_cast<double>(best->score), offsetPercent,
                     best->box[0], best->box[1], best->box[2], best->box[3],
                     static_cast<unsigned int>(results.size()),
                     static_cast<unsigned long>(inferenceMs));
        }

        // 推論結果の数値だけを残し、画像バッファは直ちに返す。
        esp_camera_fb_return(frame);
        vTaskDelay(kCaptureInterval);
    }
}

}  // namespace

extern "C" void app_main(void)
{
    runDiagnostic();
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

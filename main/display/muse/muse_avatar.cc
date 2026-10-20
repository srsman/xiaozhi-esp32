#include "muse_avatar.h"
#include "application.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cmath>
#include <cstring>

static const char* TAG = "MuseAvatar";

MuseAvatar::MuseAvatar(lv_obj_t* parent, int size_px)
    : parent_(parent), size_px_(size_px) {
    if (size_px_ > 512) {
        size_px_ = 512;
    }

    size_t buf_bytes = size_px_ * size_px_ * sizeof(uint16_t);
    frame_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frame_buffer_ == nullptr) {
        ESP_LOGW(TAG, "Failed to allocate avatar buffer in SPIRAM, falling back to default heap");
        frame_buffer_ = static_cast<uint16_t*>(malloc(buf_bytes));
    }

    if (frame_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate avatar frame buffer (%u bytes)", (unsigned int)buf_bytes);
        return;
    }

    memset(frame_buffer_, 0, buf_bytes);

    // Initialize procedural scaler size
    muse_pixel_set_size(size_px_);

    // Setup LVGL 9 image descriptor
    memset(&image_dsc_, 0, sizeof(image_dsc_));
    image_dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    image_dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
    image_dsc_.header.w = size_px_;
    image_dsc_.header.h = size_px_;
    image_dsc_.header.stride = size_px_ * sizeof(uint16_t);
    image_dsc_.data_size = buf_bytes;
    image_dsc_.data = reinterpret_cast<const uint8_t*>(frame_buffer_);

    // Create LVGL image object
    image_obj_ = lv_image_create(parent_ ? parent_ : lv_screen_active());
    lv_image_set_src(image_obj_, &image_dsc_);
    lv_obj_center(image_obj_);

    // Enable touch click events
    lv_obj_add_flag(image_obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(image_obj_, this);
    lv_obj_add_event_cb(image_obj_, TouchEventCallback, LV_EVENT_CLICKED, nullptr);

    mode_start_us_ = esp_timer_get_time();

    // Start 25fps animation render timer
    render_timer_ = lv_timer_create(TimerCallback, 40, this);
}

MuseAvatar::~MuseAvatar() {
    if (render_timer_) {
        lv_timer_delete(render_timer_);
        render_timer_ = nullptr;
    }
    if (image_obj_) {
        lv_obj_delete(image_obj_);
        image_obj_ = nullptr;
    }
    if (frame_buffer_) {
        free(frame_buffer_);
        frame_buffer_ = nullptr;
    }
}

void MuseAvatar::TimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<MuseAvatar*>(lv_timer_get_user_data(timer));
    if (self) {
        self->RenderTick();
    }
}

void MuseAvatar::TouchEventCallback(lv_event_t* e) {
    auto* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    if (!obj) return;
    auto* self = static_cast<MuseAvatar*>(lv_obj_get_user_data(obj));
    if (self) {
        self->TriggerHappy(1.0f);
        if (self->touch_cb_) {
            self->touch_cb_();
        }
    }
}

void MuseAvatar::RenderTick() {
    if (!frame_buffer_ || !image_obj_) return;

    // Automatically synchronize state with Application instance
    UpdateDeviceState(Application::GetInstance().GetDeviceState());

    int64_t now_us = esp_timer_get_time();
    float t = static_cast<float>(now_us) / 1000000.0f;
    float mode_t = static_cast<float>(now_us - mode_start_us_) / 1000000.0f;

    // Decay happy level smoothly
    if (happy_level_ > 0.0f) {
        happy_level_ -= 0.035f;
        if (happy_level_ < 0.0f) happy_level_ = 0.0f;
    }

    float live_level = audio_level_;
    if (current_mode_ == MUSE_MODE_SPEAKING) {
        speech_phase_ += 0.45f;
        if (speech_phase_ > 6.28318f) speech_phase_ -= 6.28318f;
        if (live_level <= 0.01f) {
            // Natural synthetic mouth lip-sync waveform
            live_level = std::abs(std::sin(speech_phase_)) * 0.7f + 0.2f * std::cos(speech_phase_ * 2.3f);
            if (live_level < 0.0f) live_level = 0.0f;
            if (live_level > 1.0f) live_level = 1.0f;
        }
    }

    muse_pose_t pose = {
        .mode = current_mode_,
        .t = t,
        .mode_t = mode_t,
        .level = live_level,
        .happy = happy_level_,
    };

    muse_pixel_render(&pose);
    muse_pixel_scale(frame_buffer_, size_px_, 0, size_px_ - 1, 0, size_px_ - 1);

    lv_obj_invalidate(image_obj_);
}

void MuseAvatar::UpdateDeviceState(DeviceState state) {
    muse_mode_t target_mode = current_mode_;
    switch (state) {
        case kDeviceStateStarting:
        case kDeviceStateWifiConfiguring:
        case kDeviceStateUpgrading:
            target_mode = MUSE_MODE_BOOT;
            break;
        case kDeviceStateIdle:
            target_mode = MUSE_MODE_IDLE;
            break;
        case kDeviceStateConnecting:
            target_mode = MUSE_MODE_THINKING;
            break;
        case kDeviceStateListening:
            target_mode = MUSE_MODE_LISTENING;
            break;
        case kDeviceStateSpeaking:
        case kDeviceStateNotifying:
        case kDeviceStateAudioTesting:
            target_mode = MUSE_MODE_SPEAKING;
            break;
        case kDeviceStateFatalError:
            target_mode = MUSE_MODE_ERROR;
            break;
        default:
            target_mode = MUSE_MODE_IDLE;
            break;
    }

    if (target_mode != current_mode_) {
        current_mode_ = target_mode;
        mode_start_us_ = esp_timer_get_time();
    }
}

void MuseAvatar::SetEmotion(const char* emotion) {
    if (!emotion) return;
    if (strstr(emotion, "happy") || strstr(emotion, "smile") || strstr(emotion, "excited") ||
        strstr(emotion, "joy") || strstr(emotion, "laugh") || strstr(emotion, "love")) {
        TriggerHappy(1.0f);
    }
}

void MuseAvatar::SetAudioLevel(float level) {
    audio_level_ = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
}

void MuseAvatar::TriggerHappy(float intensity) {
    happy_level_ = intensity;
}

void MuseAvatar::SetVisible(bool visible) {
    if (!image_obj_) return;
    if (visible) {
        lv_obj_remove_flag(image_obj_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(image_obj_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MuseAvatar::OnTouch(TouchCallback cb) {
    touch_cb_ = std::move(cb);
}

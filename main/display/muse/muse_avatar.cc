#include "muse_avatar.h"
#include "application.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cmath>
#include <cstring>

static const char* TAG = "MuseAvatar";

MuseAvatar::MuseAvatar(lv_obj_t* parent, int size_px, int screen_diameter)
    : parent_(parent), size_px_(size_px), screen_diameter_(screen_diameter) {
    if (size_px_ > 512) {
        size_px_ = 512;
    }

    lv_obj_t* root_screen = lv_screen_active();

    // 1. Create Outer Halo / Pulse Ring (直接挂载在主屏幕上，尺寸452px)
    int ring_d = screen_diameter_ > 20 ? (screen_diameter_ - 14) : 452;
    ring_obj_ = lv_arc_create(root_screen);
    lv_obj_set_size(ring_obj_, ring_d, ring_d);
    lv_obj_center(ring_obj_);
    lv_obj_move_background(ring_obj_);
    lv_arc_set_bg_angles(ring_obj_, 0, 360);
    lv_arc_set_rotation(ring_obj_, 270);
    lv_obj_remove_style(ring_obj_, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(ring_obj_, LV_OBJ_FLAG_CLICKABLE);

    // Track style
    lv_obj_set_style_arc_width(ring_obj_, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_obj_, lv_color_hex(0x101018), LV_PART_MAIN);
    // Indicator style
    lv_obj_set_style_arc_width(ring_obj_, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring_obj_, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring_obj_, lv_color_hex(0x5b3fd9), LV_PART_INDICATOR);

    // 2. Allocate frame buffer in SPIRAM for high-res procedural scaling
    size_t buf_bytes = size_px_ * size_px_ * sizeof(uint16_t);
    frame_buffer_ = static_cast<uint16_t*>(heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frame_buffer_ == nullptr) {
        ESP_LOGW(TAG, "Falling back to default heap for avatar buffer");
        frame_buffer_ = static_cast<uint16_t*>(malloc(buf_bytes));
    }

    if (frame_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate avatar frame buffer (%u bytes)", (unsigned int)buf_bytes);
        return;
    }

    memset(frame_buffer_, 0, buf_bytes);

    // Set scale dimension in muse engine
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

    // 3. Create avatar image centered on screen
    image_obj_ = lv_image_create(parent_ ? parent_ : root_screen);
    lv_image_set_src(image_obj_, &image_dsc_);
    lv_obj_center(image_obj_);

    // Ensure transparent background around avatar
    lv_obj_set_style_bg_opa(image_obj_, LV_OPA_TRANSP, 0);

    // Enable touch interaction
    lv_obj_add_flag(image_obj_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(image_obj_, this);
    lv_obj_add_event_cb(image_obj_, TouchEventCallback, LV_EVENT_CLICKED, nullptr);

    mode_start_us_ = esp_timer_get_time();

    // 4. Start 25fps animation render timer
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
    if (ring_obj_) {
        lv_obj_delete(ring_obj_);
        ring_obj_ = nullptr;
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

void MuseAvatar::UpdateRing(float t, float mode_t, float live_level) {
    if (!ring_obj_) return;

    if (current_mode_ == MUSE_MODE_THINKING) {
        // [用户重点需求] 思考中：环形脉冲渐变（流光霓虹跑马脉冲飞速旋转）
        float rot_speed = 360.0f; // 1秒1整圈
        int start_angle = static_cast<int>(t * rot_speed) % 360;
        int end_angle = (start_angle + 75) % 360;
        lv_arc_set_angles(ring_obj_, start_angle, end_angle);

        // 彩虹渐变霓虹流光 (天青 -> 亮紫 -> 暖金)
        float hue = fmodf(t * 140.0f, 360.0f);
        lv_color_t pulse_color = lv_color_hsv_to_rgb(static_cast<uint16_t>(hue), 85, 100);
        lv_obj_set_style_arc_color(ring_obj_, pulse_color, LV_PART_INDICATOR);
    } else if (current_mode_ == MUSE_MODE_LISTENING) {
        // 聆听中：天青色声波脉动外环
        lv_arc_set_angles(ring_obj_, 0, 360);
        uint8_t g = static_cast<uint8_t>(200 + live_level * 55);
        lv_obj_set_style_arc_color(ring_obj_, lv_color_make(60, g, 255), LV_PART_INDICATOR);
    } else if (current_mode_ == MUSE_MODE_SPEAKING) {
        // 说话中：橙暖色发声律动
        lv_arc_set_angles(ring_obj_, 0, 360);
        float pulse = 0.5f + 0.5f * std::sin(speech_phase_);
        lv_color_t speak_color = lv_color_make(255, static_cast<uint8_t>(110 + 90 * pulse), 50);
        lv_obj_set_style_arc_color(ring_obj_, speak_color, LV_PART_INDICATOR);
    } else {
        // 待机中：深紫色静谧呼吸环
        lv_arc_set_angles(ring_obj_, 0, 360);
        float breath = 0.5f + 0.5f * std::sin(t * 2.0f);
        uint8_t v = static_cast<uint8_t>(160 + 60 * breath);
        lv_obj_set_style_arc_color(ring_obj_, lv_color_make(110, 60, v), LV_PART_INDICATOR);
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

    // Update outer halo and pulse ring
    UpdateRing(t, mode_t, live_level);
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
        if (ring_obj_) lv_obj_remove_flag(ring_obj_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(image_obj_, LV_OBJ_FLAG_HIDDEN);
        if (ring_obj_) lv_obj_add_flag(ring_obj_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MuseAvatar::OnTouch(TouchCallback cb) {
    touch_cb_ = std::move(cb);
}

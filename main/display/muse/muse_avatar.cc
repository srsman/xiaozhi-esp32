#include "muse_avatar.h"
#include "application.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <cmath>
#include <cstring>
#include <cstdlib>

static const char* TAG = "MuseAvatar";

MuseAvatar::MuseAvatar(lv_obj_t* parent, int size_px, int screen_diameter)
    : parent_(parent), size_px_(size_px), screen_diameter_(screen_diameter) {
    if (size_px_ > 512) {
        size_px_ = 512;
    }

    lv_obj_t* root_screen = lv_screen_active();

    // 1. Create Outer Halo / Pulse Ring (置于顶层，确保全屏最外圈醒目可见)
    int ring_d = screen_diameter_ > 20 ? (screen_diameter_ - 10) : 456;
    ring_obj_ = lv_arc_create(root_screen);
    lv_obj_set_size(ring_obj_, ring_d, ring_d);
    lv_obj_center(ring_obj_);
    lv_obj_move_foreground(ring_obj_);
    lv_arc_set_bg_angles(ring_obj_, 0, 360);
    lv_arc_set_rotation(ring_obj_, 270);
    lv_obj_remove_style(ring_obj_, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(ring_obj_, LV_OBJ_FLAG_CLICKABLE);

    // Track style (深黑紫幽暗底轨 6px)
    lv_obj_set_style_arc_width(ring_obj_, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_obj_, lv_color_hex(0x161224), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring_obj_, LV_OPA_COVER, LV_PART_MAIN);
    // Indicator style (流光脉冲带 8px，两端半圆圆角)
    lv_obj_set_style_arc_width(ring_obj_, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring_obj_, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring_obj_, lv_color_hex(0xa855f7), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ring_obj_, LV_OPA_COVER, LV_PART_INDICATOR);

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
    lv_obj_add_event_cb(image_obj_, TouchEventCallback, LV_EVENT_ALL, nullptr);

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
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
            // 滑动手势：傲娇偏头转开
            self->TriggerAction(MUSE_ACTION_SULKY, 2.4f);
        } else if (dir == LV_DIR_TOP) {
            // 向上滑：得意欢呼大弹跳
            self->TriggerAction(MUSE_ACTION_EXCITED, 2.5f);
        } else if (dir == LV_DIR_BOTTOM) {
            // 向下滑：果冻下蹲形变
            self->TriggerAction(MUSE_ACTION_POKE, 1.8f);
        }
        if (self->touch_cb_) self->touch_cb_();
        if (self->gesture_cb_) self->gesture_cb_(dir);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        // 长按屏幕：揉眼困倦打瞌睡
        self->TriggerAction(MUSE_ACTION_SLEEPY, 3.0f);
        if (self->touch_cb_) self->touch_cb_();
    } else if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        uint32_t diff = now_ms - self->last_click_ms_;
        self->last_click_ms_ = now_ms;

        // 连续双击：挥手打招呼
        if (diff > 50 && diff < 380) {
            self->TriggerAction(MUSE_ACTION_WAVE, 2.2f);
        } else {
            // 获取触摸位置区分摸头与戳肚肚
            lv_point_t p = {0, 0};
            lv_indev_t* indev = lv_indev_active();
            if (indev) {
                lv_indev_get_point(indev, &p);
            }
            int center_y = self->screen_diameter_ / 2;
            if (p.y > 0 && p.y < (center_y - 15)) {
                // 上半部摸头：眯眼欢呼爱心
                self->TriggerAction(MUSE_ACTION_HAPPY, 2.2f);
            } else {
                // 下半部戳肚肚：果冻Q弹回弹
                self->TriggerAction(MUSE_ACTION_POKE, 1.8f);
            }
        }
        if (self->touch_cb_) self->touch_cb_();
    }
}

void MuseAvatar::UpdateRing(float t, float mode_t, float live_level) {
    if (!ring_obj_) return;

    if (current_mode_ == MUSE_MODE_THINKING) {
        // [用户核心需求] 思考中：环形脉冲渐变（流光霓虹跑马脉冲飞速旋转）
        int rot = static_cast<int>(t * 420.0f) % 360;
        lv_arc_set_rotation(ring_obj_, rot);
        lv_arc_set_angles(ring_obj_, 0, 80);

        // 彩虹渐变霓虹流光 (天青 -> 亮紫 -> 暖金)
        float phase = fmodf(t * 1.8f, 3.0f);
        lv_color_t pulse_color;
        if (phase < 1.0f) {
            float p = phase;
            pulse_color = lv_color_make(static_cast<uint8_t>(20 + 172 * p), static_cast<uint8_t>(240 - 200 * p), 255);
        } else if (phase < 2.0f) {
            float p = phase - 1.0f;
            pulse_color = lv_color_make(static_cast<uint8_t>(192 + 53 * p), static_cast<uint8_t>(40 + 118 * p), static_cast<uint8_t>(255 - 244 * p));
        } else {
            float p = phase - 2.0f;
            pulse_color = lv_color_make(static_cast<uint8_t>(245 - 225 * p), static_cast<uint8_t>(158 + 82 * p), static_cast<uint8_t>(11 + 244 * p));
        }
        lv_obj_set_style_arc_color(ring_obj_, pulse_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(ring_obj_, LV_OPA_COVER, LV_PART_INDICATOR);
    } else if (current_mode_ == MUSE_MODE_LISTENING) {
        // 聆听中：天青色声波能量波纹
        lv_arc_set_rotation(ring_obj_, 270);
        lv_arc_set_angles(ring_obj_, 0, 360);
        float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
        lv_obj_set_style_arc_color(ring_obj_, lv_color_hex(0x00f0ff), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(ring_obj_, static_cast<lv_opa_t>(180 + 75 * pulse), LV_PART_INDICATOR);
    } else if (current_mode_ == MUSE_MODE_SPEAKING) {
        // 说话中：暖金粉紫音频律动
        lv_arc_set_rotation(ring_obj_, 270);
        lv_arc_set_angles(ring_obj_, 0, 360);
        float pulse = 0.5f + 0.5f * std::sin(speech_phase_);
        lv_color_t speak_color = lv_color_make(255, static_cast<uint8_t>(120 + 80 * pulse), 60);
        lv_obj_set_style_arc_color(ring_obj_, speak_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(ring_obj_, LV_OPA_COVER, LV_PART_INDICATOR);
    } else {
        // 待机中 / 开机配网：深紫罗兰静谧呼吸光环 (常驻可见)
        lv_arc_set_rotation(ring_obj_, 270);
        lv_arc_set_angles(ring_obj_, 0, 360);
        float breath = 0.5f + 0.5f * std::sin(t * 2.5f);
        uint8_t v = static_cast<uint8_t>(150 + 95 * breath);
        lv_obj_set_style_arc_color(ring_obj_, lv_color_make(150, 70, v), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(ring_obj_, static_cast<lv_opa_t>(180 + 75 * breath), LV_PART_INDICATOR);
    }
}

void MuseAvatar::SetMode(muse_mode_t mode) {
    if (current_mode_ != mode) {
        current_mode_ = mode;
        mode_start_us_ = esp_timer_get_time();
    }
}

void MuseAvatar::RenderTick() {
    if (!frame_buffer_ || !image_obj_) return;

    // Synchronize state with Application instance when not explicitly overridden
    DeviceState app_state = Application::GetInstance().GetDeviceState();
    if (app_state == kDeviceStateConnecting) {
        SetMode(MUSE_MODE_THINKING);
    } else if (app_state == kDeviceStateListening) {
        SetMode(MUSE_MODE_LISTENING);
    } else if (app_state == kDeviceStateSpeaking || app_state == kDeviceStateAudioTesting) {
        SetMode(MUSE_MODE_SPEAKING);
    } else if (app_state == kDeviceStateIdle) {
        if (current_mode_ != MUSE_MODE_THINKING) {
            SetMode(MUSE_MODE_IDLE);
        }
    } else {
        UpdateDeviceState(app_state);
    }

    int64_t now_us = esp_timer_get_time();
    float t = static_cast<float>(now_us) / 1000000.0f;
    float mode_t = static_cast<float>(now_us - mode_start_us_) / 1000000.0f;

    // Action timer and decay
    float action_t = 0.0f;
    if (current_action_ != MUSE_ACTION_NONE) {
        action_t = static_cast<float>(now_us - action_start_us_) / 1000000.0f;
        if (action_t >= action_duration_) {
            current_action_ = MUSE_ACTION_NONE;
            action_intensity_ = 0.0f;
        } else {
            float remain = action_duration_ - action_t;
            if (remain < 0.35f) {
                action_intensity_ = remain / 0.35f;
            } else {
                action_intensity_ = 1.0f;
            }
        }
    }

    // 待机桌面灵动挂件：超过 18 秒无操作时，Avatar 随机做趣味动作（打哈欠/喷嚏/飞吻/傲娇/害羞）
    if (current_mode_ == MUSE_MODE_IDLE && current_action_ == MUSE_ACTION_NONE) {
        idle_timer_ += 0.04f;
        if (idle_timer_ > 18.0f) {
            idle_timer_ = 0.0f;
            static const muse_action_t IDLE_ACTIONS[] = {
                MUSE_ACTION_SLEEPY,
                MUSE_ACTION_SNEEZE,
                MUSE_ACTION_KISS,
                MUSE_ACTION_SULKY,
                MUSE_ACTION_SHY
            };
            int pick = (static_cast<int>(t * 7.0f)) % 5;
            TriggerAction(IDLE_ACTIONS[pick], 2.4f);
        }
    } else {
        idle_timer_ = 0.0f;
    }

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
        .action = current_action_,
        .t = t,
        .mode_t = mode_t,
        .action_t = action_t,
        .level = live_level,
        .happy = happy_level_,
        .action_intensity = action_intensity_,
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
    if (strstr(emotion, "happy") || strstr(emotion, "smile") || strstr(emotion, "joy") || strstr(emotion, "cheer")) {
        TriggerAction(MUSE_ACTION_HAPPY, 2.2f);
    } else if (strstr(emotion, "wave") || strstr(emotion, "hello") || strstr(emotion, "hi") || strstr(emotion, "bye")) {
        TriggerAction(MUSE_ACTION_WAVE, 2.0f);
    } else if (strstr(emotion, "poke") || strstr(emotion, "jelly") || strstr(emotion, "bounce") || strstr(emotion, "squish")) {
        TriggerAction(MUSE_ACTION_POKE, 1.8f);
    } else if (strstr(emotion, "sleep") || strstr(emotion, "tired") || strstr(emotion, "yawn") || strstr(emotion, "rest")) {
        TriggerAction(MUSE_ACTION_SLEEPY, 3.0f);
    } else if (strstr(emotion, "shy") || strstr(emotion, "blush") || strstr(emotion, "embarrass")) {
        TriggerAction(MUSE_ACTION_SHY, 2.5f);
    } else if (strstr(emotion, "excited") || strstr(emotion, "dance") || strstr(emotion, "jump") || strstr(emotion, "celebrate")) {
        TriggerAction(MUSE_ACTION_EXCITED, 2.5f);
    } else if (strstr(emotion, "sad") || strstr(emotion, "cry") || strstr(emotion, "sorrow") || strstr(emotion, "pout") || strstr(emotion, "tear")) {
        TriggerAction(MUSE_ACTION_SAD, 2.8f);
    } else if (strstr(emotion, "sulky") || strstr(emotion, "hmph") || strstr(emotion, "ignore") || strstr(emotion, "angry")) {
        TriggerAction(MUSE_ACTION_SULKY, 2.4f);
    } else if (strstr(emotion, "shock") || strstr(emotion, "surprise") || strstr(emotion, "wow") || strstr(emotion, "omg")) {
        TriggerAction(MUSE_ACTION_SHOCKED, 2.2f);
    } else if (strstr(emotion, "sneeze") || strstr(emotion, "achoo") || strstr(emotion, "cold")) {
        TriggerAction(MUSE_ACTION_SNEEZE, 2.2f);
    } else if (strstr(emotion, "dizzy") || strstr(emotion, "faint") || strstr(emotion, "confus") || strstr(emotion, "spin")) {
        TriggerAction(MUSE_ACTION_DIZZY, 2.8f);
    } else if (strstr(emotion, "kiss") || strstr(emotion, "love") || strstr(emotion, "heart") || strstr(emotion, "sweet")) {
        TriggerAction(MUSE_ACTION_KISS, 2.2f);
    }
}

void MuseAvatar::SetAudioLevel(float level) {
    audio_level_ = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
}

void MuseAvatar::TriggerHappy(float intensity) {
    TriggerAction(MUSE_ACTION_HAPPY, 2.0f);
}

void MuseAvatar::TriggerAction(muse_action_t action, float duration) {
    current_action_ = action;
    action_duration_ = duration;
    action_intensity_ = 1.0f;
    action_start_us_ = esp_timer_get_time();
    if (action == MUSE_ACTION_HAPPY) {
        happy_level_ = 1.0f;
    }
}

void MuseAvatar::SetVisible(bool visible) {
    if (image_obj_) {
        if (visible) {
            lv_obj_remove_flag(image_obj_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(image_obj_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (ring_obj_) {
        if (visible) {
            lv_obj_remove_flag(ring_obj_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ring_obj_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (render_timer_) {
        if (visible) {
            lv_timer_resume(render_timer_);
            mode_start_us_ = esp_timer_get_time();
        } else {
            lv_timer_pause(render_timer_);
        }
    }
}

void MuseAvatar::OnTouch(TouchCallback cb) {
    touch_cb_ = std::move(cb);
}

void MuseAvatar::OnGesture(GestureCallback cb) {
    gesture_cb_ = std::move(cb);
}

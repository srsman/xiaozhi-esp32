#ifndef MUSE_AVATAR_H
#define MUSE_AVATAR_H

#include "muse_pixel.h"
#include "device_state.h"

#include <lvgl.h>
#include <esp_timer.h>
#include <cstdint>
#include <functional>

class MuseAvatar {
public:
    using TouchCallback = std::function<void()>;
    using GestureCallback = std::function<void(lv_dir_t)>;

    explicit MuseAvatar(lv_obj_t* parent, int size_px = 280, int screen_diameter = 466);
    ~MuseAvatar();

    void UpdateDeviceState(DeviceState state);
    void SetMode(muse_mode_t mode);
    void SetEmotion(const char* emotion);
    void SetAudioLevel(float level);
    void TriggerHappy(float intensity = 1.0f);
    void TriggerAction(muse_action_t action, float duration = 2.0f);
    void SetVisible(bool visible);
    void OnTouch(TouchCallback cb);
    void OnGesture(GestureCallback cb);

    inline lv_obj_t* GetObject() const { return image_obj_; }
    inline lv_obj_t* GetRingObject() const { return ring_obj_; }
    inline muse_mode_t GetCurrentMode() const { return current_mode_; }
    inline muse_action_t GetCurrentAction() const { return current_action_; }

private:
    static void TimerCallback(lv_timer_t* timer);
    static void TouchEventCallback(lv_event_t* e);
    void RenderTick();
    void UpdateRing(float t, float mode_t, float live_level);

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* image_obj_ = nullptr;
    lv_obj_t* ring_obj_ = nullptr;
    lv_timer_t* render_timer_ = nullptr;
    int size_px_ = 280;
    int screen_diameter_ = 466;

    uint16_t* frame_buffer_ = nullptr;
    lv_image_dsc_t image_dsc_;

    muse_mode_t current_mode_ = MUSE_MODE_IDLE;
    int64_t mode_start_us_ = 0;
    float audio_level_ = 0.0f;
    float happy_level_ = 0.0f;
    float speech_phase_ = 0.0f;

    muse_action_t current_action_ = MUSE_ACTION_NONE;
    int64_t action_start_us_ = 0;
    float action_duration_ = 2.0f;
    float action_intensity_ = 0.0f;
    float idle_timer_ = 0.0f;
    uint32_t last_click_ms_ = 0;

    TouchCallback touch_cb_ = nullptr;
    GestureCallback gesture_cb_ = nullptr;
};

#endif // MUSE_AVATAR_H

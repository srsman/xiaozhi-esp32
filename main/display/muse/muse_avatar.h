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

    explicit MuseAvatar(lv_obj_t* parent, int size_px = 256);
    ~MuseAvatar();

    void UpdateDeviceState(DeviceState state);
    void SetEmotion(const char* emotion);
    void SetAudioLevel(float level);
    void TriggerHappy(float intensity = 1.0f);
    void SetVisible(bool visible);
    void OnTouch(TouchCallback cb);

    inline lv_obj_t* GetObject() const { return image_obj_; }
    inline muse_mode_t GetCurrentMode() const { return current_mode_; }

private:
    static void TimerCallback(lv_timer_t* timer);
    static void TouchEventCallback(lv_event_t* e);
    void RenderTick();

    lv_obj_t* parent_ = nullptr;
    lv_obj_t* image_obj_ = nullptr;
    lv_timer_t* render_timer_ = nullptr;
    int size_px_ = 256;

    uint16_t* frame_buffer_ = nullptr;
    lv_image_dsc_t image_dsc_;

    muse_mode_t current_mode_ = MUSE_MODE_IDLE;
    int64_t mode_start_us_ = 0;
    float audio_level_ = 0.0f;
    float happy_level_ = 0.0f;
    float speech_phase_ = 0.0f;

    TouchCallback touch_cb_ = nullptr;
};

#endif // MUSE_AVATAR_H

#ifndef WATCH_APPS_H
#define WATCH_APPS_H

#include <lvgl.h>
#include <functional>
#include <string>

// M5Stack StopWatch AMOLED Display Dimensions
#define WATCH_DISPLAY_WIDTH   466
#define WATCH_DISPLAY_HEIGHT  466
#define WATCH_CENTER_X        233
#define WATCH_CENTER_Y        233

class WatchPageManager {
public:
    using HapticCallback = std::function<void(uint16_t duration_ms)>;
    using BatteryCallback = std::function<int()>;
    using BrightnessCallback = std::function<void(uint8_t brightness)>;
    using VolumeCallback = std::function<void(uint8_t volume)>;

    static WatchPageManager& GetInstance();

    void Init(lv_obj_t* muse_screen,
              HapticCallback haptic_cb,
              BatteryCallback bat_cb,
              BrightnessCallback br_cb,
              VolumeCallback vol_cb);

    void SetImuAccel(float ax, float ay, float az);
    bool HandleBack();
    void SwitchToMuse();
    void SwitchToWatchFace();

private:
    WatchPageManager() = default;
    lv_obj_t* muse_screen_ = nullptr;
    HapticCallback haptic_cb_;
    BatteryCallback bat_cb_;
    BrightnessCallback br_cb_;
    VolumeCallback vol_cb_;
    bool is_in_muse_ = false;
};

#endif // WATCH_APPS_H

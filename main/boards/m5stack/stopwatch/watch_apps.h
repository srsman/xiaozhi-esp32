#ifndef WATCH_APPS_H
#define WATCH_APPS_H

#include <lvgl.h>
#include <functional>
#include <vector>
#include <string>

enum WatchPageId {
    WATCH_PAGE_MUSE = 0,
    WATCH_PAGE_WATCHFACE,
    WATCH_PAGE_STOPWATCH,
};

class WatchPageManager {
public:
    using HapticCallback = std::function<void(uint16_t duration_ms)>;
    using BatteryCallback = std::function<int()>; // returns 0-100%

    static WatchPageManager& GetInstance();

    void Init(lv_obj_t* muse_screen, HapticCallback haptic_cb, BatteryCallback bat_cb);
    void SwitchToPage(WatchPageId page_id, lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT);
    bool HandleBack(); // Returns true if it handled back (returned to MUSE from a sub-page)
    WatchPageId GetCurrentPage() const { return current_page_; }

    void Update1s();  // Called every 1s for clock update
    void Update50ms(); // Called every 50ms for stopwatch update

    lv_obj_t* GetWatchfaceScreen() const { return watchface_screen_; }
    lv_obj_t* GetStopwatchScreen() const { return stopwatch_screen_; }

private:
    WatchPageManager() = default;

    void CreateWatchfaceScreen();
    void CreateStopwatchScreen();
    void UpdateWatchfaceUI();
    void UpdateStopwatchUI();

    WatchPageId current_page_ = WATCH_PAGE_MUSE;
    lv_obj_t* muse_screen_ = nullptr;
    lv_obj_t* watchface_screen_ = nullptr;
    lv_obj_t* stopwatch_screen_ = nullptr;

    HapticCallback haptic_cb_ = nullptr;
    BatteryCallback bat_cb_ = nullptr;

    // Watchface widgets
    lv_obj_t* wf_time_label_ = nullptr;
    lv_obj_t* wf_sec_label_ = nullptr;
    lv_obj_t* wf_date_label_ = nullptr;
    lv_obj_t* wf_bat_label_ = nullptr;
    lv_obj_t* wf_sec_arc_ = nullptr;
    int wf_theme_color_idx_ = 0;

    // Stopwatch state & widgets
    enum StopwatchState { SW_STOPPED, SW_RUNNING, SW_PAUSED };
    StopwatchState sw_state_ = SW_STOPPED;
    uint32_t sw_start_tick_ = 0;
    uint32_t sw_elapsed_ms_ = 0;
    std::vector<uint32_t> sw_laps_;

    lv_obj_t* sw_time_label_ = nullptr;
    lv_obj_t* sw_ms_label_ = nullptr;
    lv_obj_t* sw_arc_ = nullptr;
    lv_obj_t* sw_btn_left_ = nullptr;
    lv_obj_t* sw_btn_left_label_ = nullptr;
    lv_obj_t* sw_btn_right_ = nullptr;
    lv_obj_t* sw_btn_right_label_ = nullptr;
    lv_obj_t* sw_laps_list_ = nullptr;
};

#endif // WATCH_APPS_H

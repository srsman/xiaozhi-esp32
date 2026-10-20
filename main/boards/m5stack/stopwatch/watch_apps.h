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
    WATCH_PAGE_LUCKY_WHEEL,
};

enum WatchfaceStyle {
    WF_STYLE_BIG_DIGITAL = 0,
    WF_STYLE_CLASSIC_ANALOG,
    WF_STYLE_SPORT_RING,
    WF_STYLE_MINIMAL_ZEN,
    WF_STYLE_MAX,
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

    void Update1s();   // Called every 1s for clock / watchface
    void Update30ms();  // Called every 30ms for stopwatch & wheel spinning

    lv_obj_t* GetWatchfaceScreen() const { return watchface_screen_; }
    lv_obj_t* GetStopwatchScreen() const { return stopwatch_screen_; }
    lv_obj_t* GetLuckyWheelScreen() const { return lucky_wheel_screen_; }

private:
    WatchPageManager() = default;

    // Screen builders
    void CreateWatchfaceScreen();
    void CreateStopwatchScreen();
    void CreateLuckyWheelScreen();

    // UI updaters
    void UpdateWatchfaceUI();
    void UpdateStopwatchUI();
    void UpdateLuckyWheelUI();

    // Watchface styles
    void SwitchWatchfaceStyle(WatchfaceStyle new_style);

    WatchPageId current_page_ = WATCH_PAGE_MUSE;
    lv_obj_t* muse_screen_ = nullptr;
    lv_obj_t* watchface_screen_ = nullptr;
    lv_obj_t* stopwatch_screen_ = nullptr;
    lv_obj_t* lucky_wheel_screen_ = nullptr;

    HapticCallback haptic_cb_ = nullptr;
    BatteryCallback bat_cb_ = nullptr;

    // ------------------- Watchface widgets -------------------
    WatchfaceStyle wf_style_ = WF_STYLE_BIG_DIGITAL;
    int wf_theme_color_idx_ = 0;
    lv_obj_t* wf_container_ = nullptr;
    lv_obj_t* wf_bat_label_ = nullptr;

    // Style 0: Big Digital
    lv_obj_t* wf_time_label_ = nullptr;
    lv_obj_t* wf_sec_label_ = nullptr;
    lv_obj_t* wf_date_label_ = nullptr;
    lv_obj_t* wf_sec_arc_ = nullptr;

    // Style 1: Classic Analog
    lv_obj_t* wf_analog_center_ = nullptr;
    lv_obj_t* wf_hour_hand_ = nullptr;
    lv_obj_t* wf_min_hand_ = nullptr;
    lv_obj_t* wf_sec_hand_ = nullptr;
    lv_point_precise_t hour_pts_[2] = {{0,0}, {0,0}};
    lv_point_precise_t min_pts_[2]  = {{0,0}, {0,0}};
    lv_point_precise_t sec_pts_[2]  = {{0,0}, {0,0}};

    // Style 2: Sport Ring
    lv_obj_t* wf_ring_hour_ = nullptr;
    lv_obj_t* wf_ring_min_ = nullptr;
    lv_obj_t* wf_ring_sec_ = nullptr;
    lv_obj_t* wf_sport_time_label_ = nullptr;

    // Style 3: Minimal Zen
    lv_obj_t* wf_zen_date_ = nullptr;
    lv_obj_t* wf_zen_time_ = nullptr;
    lv_obj_t* wf_zen_line_ = nullptr;

    // ------------------- Stopwatch widgets -------------------
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

    // ----------------- Lucky Wheel widgets -------------------
    lv_obj_t* wheel_arc_ = nullptr;
    lv_obj_t* wheel_result_label_ = nullptr;
    lv_obj_t* wheel_spin_btn_ = nullptr;
    lv_obj_t* wheel_pointer_ = nullptr;
    bool wheel_spinning_ = false;
    float wheel_angle_ = 0.0f;
    float wheel_speed_ = 0.0f;
    uint32_t wheel_spin_start_tick_ = 0;
    int wheel_chosen_idx_ = -1;
};

#endif // WATCH_APPS_H

#include "watch_apps.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_system.h>
#include <time.h>
#include <sys/time.h>
#include <cstdio>
#include <cmath>

#define TAG "WatchApps"

LV_FONT_DECLARE(font_noto_sans_basic_30_4);

#if defined(LV_FONT_MONTSERRAT_12) && LV_FONT_MONTSERRAT_12
LV_FONT_DECLARE(lv_font_montserrat_12);
#else
#define lv_font_montserrat_12 lv_font_montserrat_14
#endif

#if defined(LV_FONT_MONTSERRAT_16) && LV_FONT_MONTSERRAT_16
LV_FONT_DECLARE(lv_font_montserrat_16);
#else
#define lv_font_montserrat_16 lv_font_montserrat_14
#endif

#if defined(LV_FONT_MONTSERRAT_18) && LV_FONT_MONTSERRAT_18
LV_FONT_DECLARE(lv_font_montserrat_18);
#else
#define lv_font_montserrat_18 lv_font_montserrat_14
#endif

#if defined(LV_FONT_MONTSERRAT_20) && LV_FONT_MONTSERRAT_20
LV_FONT_DECLARE(lv_font_montserrat_20);
#else
#define lv_font_montserrat_20 lv_font_montserrat_14
#endif

#if defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
LV_FONT_DECLARE(lv_font_montserrat_40);
#else
#define lv_font_montserrat_40 font_noto_sans_basic_30_4
#endif

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
LV_FONT_DECLARE(lv_font_montserrat_48);
#else
#define lv_font_montserrat_48 font_noto_sans_basic_30_4
#endif

static const uint32_t THEME_COLORS[] = {
    0x38bdf8, // 科技浅青 (Cyan)
    0xf59e0b, // 活力暖橙 (Orange)
    0xa855f7, // 极光幻紫 (Purple)
    0x22c55e, // 清新翡翠 (Green)
};

static const char* WHEEL_OPTIONS[] = {
    "YES 稳了",
    "NO 慎重",
    "吃顿大餐",
    "喝杯咖啡",
    "立刻就买",
    "再等等看",
};

struct BadgeProfile {
    const char* name;
    const char* title;
    const char* quote;
    uint32_t color;
};

static const BadgeProfile BADGE_PROFILES[] = {
    {"小智探索者", "AI Architect", "Build with Intelligence", 0x38bdf8},
    {"M5 Hacker", "Hardware Maker", "Stack your Creativity", 0xf59e0b},
    {"数字游民", "Cyber Nomad", "Code, Watch & Coffee", 0x10b981},
    {"极客工程师", "Embedded Guru", "Driven by Innovation", 0xa855f7},
};

WatchPageManager& WatchPageManager::GetInstance() {
    static WatchPageManager instance;
    return instance;
}

void WatchPageManager::Init(lv_obj_t* muse_screen, HapticCallback haptic_cb, BatteryCallback bat_cb,
                            BrightnessCallback bright_cb, VolumeCallback vol_cb) {
    muse_screen_ = muse_screen;
    haptic_cb_ = haptic_cb;
    bat_cb_ = bat_cb;
    bright_cb_ = bright_cb;
    vol_cb_ = vol_cb;

    if (muse_screen_) {
        lv_obj_add_event_cb(muse_screen_, [](lv_event_t* e) {
            auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
            if (!self) return;
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }, LV_EVENT_GESTURE, this);
    }

    CreateWatchfaceScreen();
    CreateStopwatchScreen();
    CreateAlarmScreen();
    CreateFluidBoxScreen();
    CreateFftScreen();
    CreateLuckyWheelScreen();
    CreateBadgeScreen();
    CreateSettingsScreen();

    ESP_LOGI(TAG, "All Watch Apps initialized successfully.");
}

void WatchPageManager::SetImuAccel(float ax, float ay, float az) {
    imu_gx_ = -ax * 1.5f;
    imu_gy_ = ay * 1.5f;
}

void WatchPageManager::SwitchToPage(WatchPageId page_id, lv_screen_load_anim_t anim) {
    if (page_id == current_page_) return;
    lv_obj_t* target = nullptr;
    switch (page_id) {
        case WATCH_PAGE_MUSE:        target = muse_screen_; break;
        case WATCH_PAGE_WATCHFACE:   target = watchface_screen_; break;
        case WATCH_PAGE_STOPWATCH:   target = stopwatch_screen_; break;
        case WATCH_PAGE_ALARM:       target = alarm_screen_; break;
        case WATCH_PAGE_FLUIDBOX:    target = fluidbox_screen_; break;
        case WATCH_PAGE_FFT:         target = fft_screen_; break;
        case WATCH_PAGE_LUCKY_WHEEL: target = lucky_wheel_screen_; break;
        case WATCH_PAGE_BADGE:       target = badge_screen_; break;
        case WATCH_PAGE_SETTINGS:    target = settings_screen_; break;
        default: break;
    }
    if (target) {
        current_page_ = page_id;
        lv_screen_load_anim(target, anim, 250, 0, false);
        if (page_id == WATCH_PAGE_WATCHFACE) UpdateWatchfaceUI();
    }
}

bool WatchPageManager::HandleBack() {
    if (current_page_ != WATCH_PAGE_MUSE) {
        if (haptic_cb_) haptic_cb_(30);
        SwitchToPage(WATCH_PAGE_MUSE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        return true;
    }
    return false;
}

// -------------------------------------------------------------
// 1. WATCHFACE (表盘)
// -------------------------------------------------------------
void WatchPageManager::CreateWatchfaceScreen() {
    watchface_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(watchface_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(watchface_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(watchface_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(watchface_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_STOPWATCH, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_MUSE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        } else if (code == LV_EVENT_LONG_PRESSED) {
            int next = ((int)self->wf_style_ + 1) % WF_STYLE_MAX;
            self->SwitchWatchfaceStyle((WatchfaceStyle)next);
            if (self->haptic_cb_) self->haptic_cb_(60);
        }
    }, LV_EVENT_ALL, this);

    wf_container_ = lv_obj_create(watchface_screen_);
    lv_obj_set_size(wf_container_, 280, 280);
    lv_obj_center(wf_container_);
    lv_obj_set_style_bg_opa(wf_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wf_container_, 0, 0);
    lv_obj_remove_flag(wf_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wf_container_, LV_OBJ_FLAG_EVENT_BUBBLE);

    wf_bat_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_bat_label_, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_set_style_text_font(wf_bat_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wf_bat_label_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(wf_bat_label_, "BAT 100%");

    SwitchWatchfaceStyle(WF_STYLE_BIG_DIGITAL);
}

void WatchPageManager::SwitchWatchfaceStyle(WatchfaceStyle new_style) {
    wf_style_ = new_style;
    lv_obj_clean(wf_container_);

    wf_time_label_ = nullptr;
    wf_sec_label_ = nullptr;
    wf_date_label_ = nullptr;
    wf_sec_arc_ = nullptr;
    wf_hour_hand_ = nullptr;
    wf_min_hand_ = nullptr;
    wf_sec_hand_ = nullptr;
    wf_ring_hour_ = nullptr;
    wf_ring_min_ = nullptr;
    wf_ring_sec_ = nullptr;
    wf_sport_time_label_ = nullptr;
    wf_zen_date_ = nullptr;
    wf_zen_time_ = nullptr;

    uint32_t color = THEME_COLORS[wf_theme_color_idx_];

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        wf_sec_arc_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_sec_arc_, 272, 272);
        lv_obj_center(wf_sec_arc_);
        lv_arc_set_rotation(wf_sec_arc_, 270);
        lv_arc_set_bg_angles(wf_sec_arc_, 0, 360);
        lv_arc_set_range(wf_sec_arc_, 0, 60);
        lv_obj_remove_style(wf_sec_arc_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_sec_arc_, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_sec_arc_, 4, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(color), LV_PART_INDICATOR);
        lv_obj_remove_flag(wf_sec_arc_, LV_OBJ_FLAG_CLICKABLE);

        wf_date_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_date_label_, LV_ALIGN_CENTER, 0, -56);
        lv_obj_set_style_text_font(wf_date_label_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(wf_date_label_, lv_color_hex(0x94a3b8), 0);

        wf_time_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_time_label_, LV_ALIGN_CENTER, 0, -4);
        lv_obj_set_style_text_font(wf_time_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(wf_time_label_, lv_color_hex(0xffffff), 0);

        wf_sec_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_sec_label_, LV_ALIGN_CENTER, 0, 52);
        lv_obj_set_style_text_font(wf_sec_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(wf_sec_label_, lv_color_hex(color), 0);
    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        lv_obj_t* dial = lv_obj_create(wf_container_);
        lv_obj_set_size(dial, 270, 270);
        lv_obj_center(dial);
        lv_obj_set_style_bg_opa(dial, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(dial, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(dial, 2, 0);
        lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < 12; ++i) {
            float angle = i * 30.0f * 3.14159265f / 180.0f;
            int x = 140 + (int)(118 * sinf(angle));
            int y = 140 - (int)(118 * cosf(angle));
            lv_obj_t* dot = lv_obj_create(wf_container_);
            int d_sz = (i % 3 == 0) ? 6 : 3;
            lv_obj_set_size(dot, d_sz, d_sz);
            lv_obj_set_pos(dot, x - d_sz / 2, y - d_sz / 2);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_bg_color(dot, lv_color_hex((i % 3 == 0) ? color : 0x64748b), 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        }

        wf_hour_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_hour_hand_, 5, 0);
        lv_obj_set_style_line_color(wf_hour_hand_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_line_rounded(wf_hour_hand_, true, 0);

        wf_min_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_min_hand_, 3, 0);
        lv_obj_set_style_line_color(wf_min_hand_, lv_color_hex(0xe2e8f0), 0);
        lv_obj_set_style_line_rounded(wf_min_hand_, true, 0);

        wf_sec_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_sec_hand_, 2, 0);
        lv_obj_set_style_line_color(wf_sec_hand_, lv_color_hex(color), 0);
        lv_obj_set_style_line_rounded(wf_sec_hand_, true, 0);

        lv_obj_t* center_dot = lv_obj_create(wf_container_);
        lv_obj_set_size(center_dot, 10, 10);
        lv_obj_center(center_dot);
        lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(center_dot, 0, 0);
        lv_obj_set_style_bg_color(center_dot, lv_color_hex(color), 0);
    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        wf_ring_hour_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_hour_, 260, 260);
        lv_obj_center(wf_ring_hour_);
        lv_arc_set_rotation(wf_ring_hour_, 270);
        lv_arc_set_bg_angles(wf_ring_hour_, 0, 360);
        lv_arc_set_range(wf_ring_hour_, 0, 24);
        lv_obj_remove_style(wf_ring_hour_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_hour_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_hour_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0xf43f5e), LV_PART_INDICATOR);

        wf_ring_min_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_min_, 226, 226);
        lv_obj_center(wf_ring_min_);
        lv_arc_set_rotation(wf_ring_min_, 270);
        lv_arc_set_bg_angles(wf_ring_min_, 0, 360);
        lv_arc_set_range(wf_ring_min_, 0, 60);
        lv_obj_remove_style(wf_ring_min_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_min_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_min_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x10b981), LV_PART_INDICATOR);

        wf_ring_sec_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_sec_, 192, 192);
        lv_obj_center(wf_ring_sec_);
        lv_arc_set_rotation(wf_ring_sec_, 270);
        lv_arc_set_bg_angles(wf_ring_sec_, 0, 360);
        lv_arc_set_range(wf_ring_sec_, 0, 60);
        lv_obj_remove_style(wf_ring_sec_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_sec_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_sec_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

        wf_sport_time_label_ = lv_label_create(wf_container_);
        lv_obj_center(wf_sport_time_label_);
        lv_obj_set_style_text_font(wf_sport_time_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(wf_sport_time_label_, lv_color_hex(0xffffff), 0);
    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        wf_zen_time_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_time_, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_style_text_font(wf_zen_time_, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(wf_zen_time_, lv_color_hex(0xf8fafc), 0);

        wf_zen_date_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_date_, LV_ALIGN_CENTER, 0, 42);
        lv_obj_set_style_text_font(wf_zen_date_, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(wf_zen_date_, lv_color_hex(color), 0);
    }

    UpdateWatchfaceUI();
}

void WatchPageManager::UpdateWatchfaceUI() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    if (wf_bat_label_ && bat_cb_) {
        char bat_buf[32];
        snprintf(bat_buf, sizeof(bat_buf), "BAT %d%%", bat_cb_());
        lv_label_set_text(wf_bat_label_, bat_buf);
    }

    char time_str[32], date_str[32], sec_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", ti.tm_hour, ti.tm_min);
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    snprintf(sec_str, sizeof(sec_str), "%02d", ti.tm_sec);

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        if (wf_time_label_) lv_label_set_text(wf_time_label_, time_str);
        if (wf_date_label_) lv_label_set_text(wf_date_label_, date_str);
        if (wf_sec_label_)  lv_label_set_text(wf_sec_label_, sec_str);
        if (wf_sec_arc_)    lv_arc_set_value(wf_sec_arc_, ti.tm_sec);
    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        float h_angle = (ti.tm_hour % 12 + ti.tm_min / 60.0f) * 30.0f * 3.14159265f / 180.0f;
        float m_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f * 3.14159265f / 180.0f;
        float s_angle = (ti.tm_sec) * 6.0f * 3.14159265f / 180.0f;

        hour_pts_[0] = {140, 140};
        hour_pts_[1] = {(int32_t)(140 + 52 * sinf(h_angle)), (int32_t)(140 - 52 * cosf(h_angle))};
        min_pts_[0]  = {140, 140};
        min_pts_[1]  = {(int32_t)(140 + 78 * sinf(m_angle)), (int32_t)(140 - 78 * cosf(m_angle))};
        sec_pts_[0]  = {140, 140};
        sec_pts_[1]  = {(int32_t)(140 + 96 * sinf(s_angle)), (int32_t)(140 - 96 * cosf(s_angle))};

        if (wf_hour_hand_) lv_line_set_points(wf_hour_hand_, hour_pts_, 2);
        if (wf_min_hand_)  lv_line_set_points(wf_min_hand_, min_pts_, 2);
        if (wf_sec_hand_)  lv_line_set_points(wf_sec_hand_, sec_pts_, 2);
    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        if (wf_ring_hour_) lv_arc_set_value(wf_ring_hour_, ti.tm_hour);
        if (wf_ring_min_)  lv_arc_set_value(wf_ring_min_, ti.tm_min);
        if (wf_ring_sec_)  lv_arc_set_value(wf_ring_sec_, ti.tm_sec);
        if (wf_sport_time_label_) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
            lv_label_set_text(wf_sport_time_label_, buf);
        }
    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        if (wf_zen_time_) lv_label_set_text(wf_zen_time_, time_str);
        if (wf_zen_date_) lv_label_set_text(wf_zen_date_, date_str);
    }
}

// -------------------------------------------------------------
// 2. STOPWATCH & COUNTDOWN TIMER (秒表与倒计时器)
// -------------------------------------------------------------
void WatchPageManager::CreateStopwatchScreen() {
    stopwatch_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(stopwatch_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(stopwatch_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(stopwatch_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(stopwatch_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_ALARM, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }
    }, LV_EVENT_ALL, this);

    // 顶部模式切换按钮
    sw_mode_tab_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_mode_tab_, 110, 28);
    lv_obj_align(sw_mode_tab_, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_radius(sw_mode_tab_, 14, 0);
    lv_obj_set_style_bg_color(sw_mode_tab_, lv_color_hex(0x1e293b), 0);
    lv_obj_t* tab_txt = lv_label_create(sw_mode_tab_);
    lv_obj_center(tab_txt);
    lv_obj_set_style_text_font(tab_txt, &lv_font_montserrat_12, 0);
    lv_label_set_text(tab_txt, "秒表 / 倒计时");
    lv_obj_add_event_cb(sw_mode_tab_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        self->sw_mode_ = (self->sw_mode_ == MODE_STOPWATCH) ? MODE_COUNTDOWN : MODE_STOPWATCH;
        if (self->haptic_cb_) self->haptic_cb_(30);
        if (self->sw_mode_ == MODE_COUNTDOWN) {
            lv_label_set_text(self->sw_btn_left_label_, "3分钟");
            lv_label_set_text(self->sw_btn_right_label_, self->cd_running_ ? "暂停" : "开始");
            lv_label_set_text(self->sw_time_label_, "03:00");
            lv_label_set_text(self->sw_ms_label_, "倒计时");
        } else {
            lv_label_set_text(self->sw_btn_left_label_, (self->sw_state_ == SW_RUNNING) ? "计圈" : "复位");
            lv_label_set_text(self->sw_btn_right_label_, (self->sw_state_ == SW_RUNNING) ? "停止" : "启动");
            lv_label_set_text(self->sw_time_label_, "00:00");
            lv_label_set_text(self->sw_ms_label_, ".00");
        }
    }, LV_EVENT_CLICKED, this);

    sw_arc_ = lv_arc_create(stopwatch_screen_);
    lv_obj_set_size(sw_arc_, 264, 264);
    lv_obj_center(sw_arc_);
    lv_arc_set_rotation(sw_arc_, 270);
    lv_arc_set_bg_angles(sw_arc_, 0, 360);
    lv_arc_set_range(sw_arc_, 0, 60000);
    lv_obj_remove_style(sw_arc_, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(sw_arc_, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(sw_arc_, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

    sw_time_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_time_label_, LV_ALIGN_CENTER, -16, -26);
    lv_obj_set_style_text_font(sw_time_label_, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(sw_time_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(sw_time_label_, "00:00");

    sw_ms_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_ms_label_, LV_ALIGN_CENTER, 64, -22);
    lv_obj_set_style_text_font(sw_ms_label_, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(sw_ms_label_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(sw_ms_label_, ".00");

    sw_btn_left_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_left_, 76, 38);
    lv_obj_align(sw_btn_left_, LV_ALIGN_BOTTOM_LEFT, 44, -36);
    lv_obj_set_style_radius(sw_btn_left_, 19, 0);
    lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x334155), 0);
    sw_btn_left_label_ = lv_label_create(sw_btn_left_);
    lv_obj_center(sw_btn_left_label_);
    lv_obj_set_style_text_font(sw_btn_left_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(sw_btn_left_label_, "复位");

    lv_obj_add_event_cb(sw_btn_left_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (self->haptic_cb_) self->haptic_cb_(30);
        if (self->sw_mode_ == MODE_COUNTDOWN) {
            // 循环切换倒计时预设 (1m -> 3m -> 5m)
            if (self->cd_total_sec_ == 60) self->cd_total_sec_ = 180;
            else if (self->cd_total_sec_ == 180) self->cd_total_sec_ = 300;
            else self->cd_total_sec_ = 60;
            self->cd_remain_sec_ = self->cd_total_sec_;
            char b[16];
            snprintf(b, sizeof(b), "%dm", (int)(self->cd_total_sec_ / 60));
            lv_label_set_text(self->sw_btn_left_label_, b);
            snprintf(b, sizeof(b), "%02d:00", (int)(self->cd_total_sec_ / 60));
            lv_label_set_text(self->sw_time_label_, b);
        } else {
            if (self->sw_state_ == SW_RUNNING) {
                if (self->sw_laps_.size() < 10) self->sw_laps_.push_back(self->sw_elapsed_ms_);
            } else {
                self->sw_state_ = SW_STOPPED;
                self->sw_elapsed_ms_ = 0;
                self->sw_laps_.clear();
                lv_label_set_text(self->sw_time_label_, "00:00");
                lv_label_set_text(self->sw_ms_label_, ".00");
                lv_arc_set_value(self->sw_arc_, 0);
                lv_label_set_text(self->sw_btn_left_label_, "复位");
                lv_label_set_text(self->sw_btn_right_label_, "启动");
            }
        }
    }, LV_EVENT_CLICKED, this);

    sw_btn_right_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_right_, 76, 38);
    lv_obj_align(sw_btn_right_, LV_ALIGN_BOTTOM_RIGHT, -44, -36);
    lv_obj_set_style_radius(sw_btn_right_, 19, 0);
    lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x059669), 0);
    sw_btn_right_label_ = lv_label_create(sw_btn_right_);
    lv_obj_center(sw_btn_right_label_);
    lv_obj_set_style_text_font(sw_btn_right_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(sw_btn_right_label_, "启动");

    lv_obj_add_event_cb(sw_btn_right_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (self->haptic_cb_) self->haptic_cb_(40);
        if (self->sw_mode_ == MODE_COUNTDOWN) {
            self->cd_running_ = !self->cd_running_;
            lv_label_set_text(self->sw_btn_right_label_, self->cd_running_ ? "暂停" : "开始");
        } else {
            if (self->sw_state_ == SW_RUNNING) {
                self->sw_state_ = SW_PAUSED;
                lv_label_set_text(self->sw_btn_right_label_, "继续");
                lv_label_set_text(self->sw_btn_left_label_, "复位");
                lv_obj_set_style_bg_color(self->sw_btn_right_, lv_color_hex(0x059669), 0);
            } else {
                self->sw_state_ = SW_RUNNING;
                self->sw_start_tick_ = esp_timer_get_time() / 1000 - self->sw_elapsed_ms_;
                lv_label_set_text(self->sw_btn_right_label_, "停止");
                lv_label_set_text(self->sw_btn_left_label_, "计圈");
                lv_obj_set_style_bg_color(self->sw_btn_right_, lv_color_hex(0xd97706), 0);
            }
        }
    }, LV_EVENT_CLICKED, this);
}

void WatchPageManager::UpdateStopwatchUI() {
    if (sw_mode_ == MODE_COUNTDOWN) return;
    if (sw_state_ == SW_RUNNING) {
        sw_elapsed_ms_ = (esp_timer_get_time() / 1000) - sw_start_tick_;
        uint32_t total_sec = sw_elapsed_ms_ / 1000;
        uint32_t min = (total_sec / 60) % 100;
        uint32_t sec = total_sec % 60;
        uint32_t ms = (sw_elapsed_ms_ % 1000) / 10;

        char time_b[16], ms_b[8];
        snprintf(time_b, sizeof(time_b), "%02u:%02u", min, sec);
        snprintf(ms_b, sizeof(ms_b), ".%02u", ms);

        if (sw_time_label_) lv_label_set_text(sw_time_label_, time_b);
        if (sw_ms_label_)   lv_label_set_text(sw_ms_label_, ms_b);
        if (sw_arc_)        lv_arc_set_value(sw_arc_, sw_elapsed_ms_ % 60000);
    }
}

// -------------------------------------------------------------
// 3. ALARM (闹钟)
// -------------------------------------------------------------
void WatchPageManager::CreateAlarmScreen() {
    alarm_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(alarm_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(alarm_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(alarm_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(alarm_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_FLUIDBOX, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_STOPWATCH, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }
    }, LV_EVENT_ALL, this);

    lv_obj_t* title = lv_label_create(alarm_screen_);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "定时闹钟");

    alarm_time_label_ = lv_label_create(alarm_screen_);
    lv_obj_align(alarm_time_label_, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_text_font(alarm_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(alarm_time_label_, lv_color_hex(0x38bdf8), 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour_, alarm_min_);
    lv_label_set_text(alarm_time_label_, buf);

    // 时钟调整按键
    lv_obj_t* btn_h_plus = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_h_plus, 50, 32);
    lv_obj_align(btn_h_plus, LV_ALIGN_CENTER, -55, 36);
    lv_obj_set_style_radius(btn_h_plus, 16, 0);
    lv_obj_set_style_bg_color(btn_h_plus, lv_color_hex(0x1e293b), 0);
    lv_obj_t* t_hp = lv_label_create(btn_h_plus);
    lv_obj_center(t_hp);
    lv_label_set_text(t_hp, "+时");
    lv_obj_add_event_cb(btn_h_plus, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_hour_ = (self->alarm_hour_ + 1) % 24;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_m_plus = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_m_plus, 50, 32);
    lv_obj_align(btn_m_plus, LV_ALIGN_CENTER, 55, 36);
    lv_obj_set_style_radius(btn_m_plus, 16, 0);
    lv_obj_set_style_bg_color(btn_m_plus, lv_color_hex(0x1e293b), 0);
    lv_obj_t* t_mp = lv_label_create(btn_m_plus);
    lv_obj_center(t_mp);
    lv_label_set_text(t_mp, "+分");
    lv_obj_add_event_cb(btn_m_plus, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_min_ = (self->alarm_min_ + 5) % 60;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    // 闹钟开关
    alarm_switch_ = lv_switch_create(alarm_screen_);
    lv_obj_align(alarm_switch_, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_set_size(alarm_switch_, 56, 28);
    lv_obj_add_event_cb(alarm_switch_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_enabled_ = lv_obj_has_state(self->alarm_switch_, LV_STATE_CHECKED);
            if (self->haptic_cb_) self->haptic_cb_(40);
        }
    }, LV_EVENT_VALUE_CHANGED, this);
}

void WatchPageManager::UpdateAlarmUI() {
    if (alarm_time_label_) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour_, alarm_min_);
        lv_label_set_text(alarm_time_label_, buf);
    }
}

// -------------------------------------------------------------
// 4. FLUIDBOX (重力流体沙漏)
// -------------------------------------------------------------
void WatchPageManager::CreateFluidBoxScreen() {
    fluidbox_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(fluidbox_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(fluidbox_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(fluidbox_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(fluidbox_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_FFT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_ALARM, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }
    }, LV_EVENT_ALL, this);

    fluid_ring_ = lv_obj_create(fluidbox_screen_);
    lv_obj_set_size(fluid_ring_, 260, 260);
    lv_obj_center(fluid_ring_);
    lv_obj_set_style_bg_opa(fluid_ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(fluid_ring_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(fluid_ring_, 2, 0);
    lv_obj_set_style_radius(fluid_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(fluid_ring_, LV_OBJ_FLAG_SCROLLABLE);

    particles_.clear();
    const uint32_t colors[] = {0x38bdf8, 0x06b6d4, 0x3b82f6, 0x60a5fa, 0x93c5fd};
    for (int i = 0; i < FLUID_PARTICLE_COUNT; ++i) {
        FluidParticle p;
        p.x = 100.0f + (esp_random() % 80);
        p.y = 80.0f + (esp_random() % 80);
        p.vx = 0.0f;
        p.vy = 0.0f;
        p.color = colors[i % 5];
        p.obj = lv_obj_create(fluidbox_screen_);
        lv_obj_set_size(p.obj, 10, 10);
        lv_obj_set_pos(p.obj, (int)p.x, (int)p.y);
        lv_obj_set_style_radius(p.obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(p.obj, 0, 0);
        lv_obj_set_style_bg_color(p.obj, lv_color_hex(p.color), 0);
        lv_obj_remove_flag(p.obj, LV_OBJ_FLAG_CLICKABLE);
        particles_.push_back(p);
    }

    lv_obj_t* hint = lv_label_create(fluidbox_screen_);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748b), 0);
    lv_label_set_text(hint, "倾斜手表感受重力流体");
}

void WatchPageManager::UpdateFluidBoxUI() {
    const float cx = 140.0f;
    const float cy = 140.0f;
    const float max_r = 118.0f;

    for (auto& p : particles_) {
        // 施加重力加速度与黏滞阻尼
        p.vx += imu_gx_ * 0.12f;
        p.vy += imu_gy_ * 0.12f;
        p.vx *= 0.96f;
        p.vy *= 0.96f;

        p.x += p.vx;
        p.y += p.vy;

        // 碰撞圆形边界检测
        float dx = p.x - cx;
        float dy = p.y - cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > max_r) {
            float nx = dx / dist;
            float ny = dy / dist;
            p.x = cx + nx * max_r;
            p.y = cy + ny * max_r;

            // 弹性反弹
            float dot = p.vx * nx + p.vy * ny;
            p.vx = (p.vx - 1.6f * dot * nx) * 0.6f;
            p.vy = (p.vy - 1.6f * dot * ny) * 0.6f;
        }

        if (p.obj) {
            lv_obj_set_pos(p.obj, (int)(p.x - 5), (int)(p.y - 5));
        }
    }
}

// -------------------------------------------------------------
// 5. AUDIO FFT SPECTRUM (音频律动频谱)
// -------------------------------------------------------------
void WatchPageManager::CreateFftScreen() {
    fft_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(fft_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(fft_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(fft_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(fft_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_LUCKY_WHEEL, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_FLUIDBOX, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }
    }, LV_EVENT_ALL, this);

    lv_obj_t* title = lv_label_create(fft_screen_);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "音频频谱");

    // 黑胶中心小圆盘
    lv_obj_t* disc = lv_obj_create(fft_screen_);
    lv_obj_set_size(disc, 70, 70);
    lv_obj_center(disc);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0x18181b), 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(0xa855f7), 0);
    lv_obj_set_style_border_width(disc, 2, 0);

    // 16 根频谱跳动柱
    const int start_x = 36;
    const int gap = 13;
    for (int i = 0; i < FFT_BAR_COUNT; ++i) {
        fft_bars_[i] = lv_obj_create(fft_screen_);
        lv_obj_set_size(fft_bars_[i], 9, 20);
        lv_obj_set_pos(fft_bars_[i], start_x + i * gap, 160);
        lv_obj_set_style_radius(fft_bars_[i], 4, 0);
        lv_obj_set_style_border_width(fft_bars_[i], 0, 0);
        uint32_t col = (i < 5) ? 0x10b981 : (i < 11 ? 0x38bdf8 : 0xf43f5e);
        lv_obj_set_style_bg_color(fft_bars_[i], lv_color_hex(col), 0);
    }
}

void WatchPageManager::UpdateFftUI() {
    for (int i = 0; i < FFT_BAR_COUNT; ++i) {
        if (!fft_bars_[i]) continue;
        int target_h = 10 + (esp_random() % 90);
        fft_heights_[i] = (int)(fft_heights_[i] * 0.6f + target_h * 0.4f);
        lv_obj_set_size(fft_bars_[i], 9, fft_heights_[i]);
        lv_obj_set_y(fft_bars_[i], 180 - fft_heights_[i]);
    }
}

// -------------------------------------------------------------
// 6. LUCKY WHEEL (幸运大转盘)
// -------------------------------------------------------------
void WatchPageManager::CreateLuckyWheelScreen() {
    lucky_wheel_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(lucky_wheel_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lucky_wheel_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(lucky_wheel_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(lucky_wheel_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_BADGE, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_FFT, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        }
    }, LV_EVENT_ALL, this);

    wheel_arc_ = lv_arc_create(lucky_wheel_screen_);
    lv_obj_set_size(wheel_arc_, 250, 250);
    lv_obj_center(wheel_arc_);
    lv_arc_set_rotation(wheel_arc_, 0);
    lv_arc_set_bg_angles(wheel_arc_, 0, 360);
    lv_arc_set_range(wheel_arc_, 0, 360);
    lv_obj_remove_style(wheel_arc_, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(wheel_arc_, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(wheel_arc_, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0xf59e0b), LV_PART_INDICATOR);

    wheel_result_label_ = lv_label_create(lucky_wheel_screen_);
    lv_obj_align(wheel_result_label_, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_text_font(wheel_result_label_, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(wheel_result_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(wheel_result_label_, "做个决定？");

    wheel_spin_btn_ = lv_btn_create(lucky_wheel_screen_);
    lv_obj_set_size(wheel_spin_btn_, 100, 44);
    lv_obj_align(wheel_spin_btn_, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_radius(wheel_spin_btn_, 22, 0);
    lv_obj_set_style_bg_color(wheel_spin_btn_, lv_color_hex(0xd97706), 0);
    lv_obj_t* btn_lbl = lv_label_create(wheel_spin_btn_);
    lv_obj_center(btn_lbl);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(btn_lbl, "SPIN 转！");

    lv_obj_add_event_cb(wheel_spin_btn_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && !self->wheel_spinning_) {
            self->wheel_spinning_ = true;
            self->wheel_speed_ = 35.0f + (esp_random() % 25);
            self->wheel_chosen_idx_ = esp_random() % 6;
            lv_label_set_text(self->wheel_result_label_, "摇号中...");
            if (self->haptic_cb_) self->haptic_cb_(60);
        }
    }, LV_EVENT_CLICKED, this);
}

void WatchPageManager::UpdateLuckyWheelUI() {
    if (!wheel_spinning_) return;
    wheel_angle_ += wheel_speed_;
    if (wheel_angle_ >= 360.0f) wheel_angle_ -= 360.0f;
    lv_arc_set_value(wheel_arc_, (int)wheel_angle_);

    wheel_speed_ *= 0.97f;
    if (wheel_speed_ < 0.6f) {
        wheel_spinning_ = false;
        wheel_speed_ = 0.0f;
        lv_label_set_text(wheel_result_label_, WHEEL_OPTIONS[wheel_chosen_idx_]);
        if (haptic_cb_) haptic_cb_(100);
    }
}

// -------------------------------------------------------------
// 7. BADGE (极客电子工牌)
// -------------------------------------------------------------
void WatchPageManager::CreateBadgeScreen() {
    badge_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(badge_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(badge_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(badge_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(badge_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_LUCKY_WHEEL, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
        } else if (code == LV_EVENT_CLICKED) {
            self->badge_slot_ = (self->badge_slot_ + 1) % 4;
            const auto& p = BADGE_PROFILES[self->badge_slot_];
            lv_label_set_text(self->badge_name_label_, p.name);
            lv_label_set_text(self->badge_title_label_, p.title);
            lv_label_set_text(self->badge_desc_label_, p.quote);
            lv_obj_set_style_border_color(self->badge_card_, lv_color_hex(p.color), 0);
            if (self->haptic_cb_) self->haptic_cb_(40);
        }
    }, LV_EVENT_ALL, this);

    badge_card_ = lv_obj_create(badge_screen_);
    lv_obj_set_size(badge_card_, 230, 200);
    lv_obj_center(badge_card_);
    lv_obj_set_style_bg_color(badge_card_, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_border_color(badge_card_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_border_width(badge_card_, 2, 0);
    lv_obj_set_style_radius(badge_card_, 24, 0);
    lv_obj_remove_flag(badge_card_, LV_OBJ_FLAG_SCROLLABLE);

    badge_name_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_name_label_, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_font(badge_name_label_, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(badge_name_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(badge_name_label_, BADGE_PROFILES[0].name);

    badge_title_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_title_label_, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_text_font(badge_title_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_title_label_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(badge_title_label_, BADGE_PROFILES[0].title);

    badge_desc_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_desc_label_, LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_text_font(badge_desc_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(badge_desc_label_, lv_color_hex(0x94a3b8), 0);
    lv_label_set_text(badge_desc_label_, BADGE_PROFILES[0].quote);

    lv_obj_t* hint = lv_label_create(badge_screen_);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748b), 0);
    lv_label_set_text(hint, "轻触名片切换身份");
}

// -------------------------------------------------------------
// 8. SETTINGS & CONTROL CENTER (控制中心)
// -------------------------------------------------------------
void WatchPageManager::CreateSettingsScreen() {
    settings_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(settings_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(settings_screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(settings_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_TOP || dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_MUSE, LV_SCR_LOAD_ANIM_MOVE_TOP);
            }
        }
    }, LV_EVENT_ALL, this);

    lv_obj_t* title = lv_label_create(settings_screen_);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "控制中心");

    // 亮度调节滑块
    lv_obj_t* b_lbl = lv_label_create(settings_screen_);
    lv_obj_align(b_lbl, LV_ALIGN_TOP_LEFT, 34, 66);
    lv_obj_set_style_text_color(b_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(b_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(b_lbl, "亮度");

    bright_slider_ = lv_slider_create(settings_screen_);
    lv_obj_set_size(bright_slider_, 140, 16);
    lv_obj_align(bright_slider_, LV_ALIGN_TOP_LEFT, 88, 68);
    lv_slider_set_range(bright_slider_, 10, 100);
    lv_slider_set_value(bright_slider_, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(bright_slider_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->bright_cb_) {
            self->bright_cb_((uint8_t)lv_slider_get_value(self->bright_slider_));
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // 音量调节滑块
    lv_obj_t* v_lbl = lv_label_create(settings_screen_);
    lv_obj_align(v_lbl, LV_ALIGN_TOP_LEFT, 34, 106);
    lv_obj_set_style_text_color(v_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(v_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(v_lbl, "音量");

    vol_slider_ = lv_slider_create(settings_screen_);
    lv_obj_set_size(vol_slider_, 140, 16);
    lv_obj_align(vol_slider_, LV_ALIGN_TOP_LEFT, 88, 108);
    lv_slider_set_range(vol_slider_, 0, 100);
    lv_slider_set_value(vol_slider_, 70, LV_ANIM_OFF);
    lv_obj_add_event_cb(vol_slider_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->vol_cb_) {
            self->vol_cb_((uint8_t)lv_slider_get_value(self->vol_slider_));
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // 马达触觉震动测试按键
    lv_obj_t* haptic_btn = lv_btn_create(settings_screen_);
    lv_obj_set_size(haptic_btn, 96, 32);
    lv_obj_align(haptic_btn, LV_ALIGN_CENTER, -54, 26);
    lv_obj_set_style_radius(haptic_btn, 16, 0);
    lv_obj_set_style_bg_color(haptic_btn, lv_color_hex(0x1e293b), 0);
    lv_obj_t* hb_txt = lv_label_create(haptic_btn);
    lv_obj_center(hb_txt);
    lv_obj_set_style_text_font(hb_txt, &lv_font_montserrat_12, 0);
    lv_label_set_text(hb_txt, "马达震动");
    lv_obj_add_event_cb(haptic_btn, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->haptic_cb_) self->haptic_cb_(100);
    }, LV_EVENT_CLICKED, this);

    // 重启设备按键
    lv_obj_t* reboot_btn = lv_btn_create(settings_screen_);
    lv_obj_set_size(reboot_btn, 96, 32);
    lv_obj_align(reboot_btn, LV_ALIGN_CENTER, 54, 26);
    lv_obj_set_style_radius(reboot_btn, 16, 0);
    lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0x450a0a), 0);
    lv_obj_t* rb_txt = lv_label_create(reboot_btn);
    lv_obj_center(rb_txt);
    lv_obj_set_style_text_font(rb_txt, &lv_font_montserrat_12, 0);
    lv_label_set_text(rb_txt, "重启手表");
    lv_obj_add_event_cb(reboot_btn, [](lv_event_t* e) {
        esp_restart();
    }, LV_EVENT_CLICKED, this);

    dev_info_label_ = lv_label_create(settings_screen_);
    lv_obj_align(dev_info_label_, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_text_color(dev_info_label_, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_text_font(dev_info_label_, &lv_font_montserrat_12, 0);
    lv_label_set_text(dev_info_label_, "M5Stack StopWatch v2.5.4\nRTC / IMU / FluidBox / FFT");
}

// -------------------------------------------------------------
// 定时循环刷新 (1s & 30ms)
// -------------------------------------------------------------
void WatchPageManager::Update1s() {
    if (current_page_ == WATCH_PAGE_WATCHFACE) {
        UpdateWatchfaceUI();
    }

    // 倒计时器 1 秒递减
    if (sw_mode_ == MODE_COUNTDOWN && cd_running_ && cd_remain_sec_ > 0) {
        cd_remain_sec_--;
        char b[16];
        snprintf(b, sizeof(b), "%02d:%02d", (int)(cd_remain_sec_ / 60), (int)(cd_remain_sec_ % 60));
        if (sw_time_label_) lv_label_set_text(sw_time_label_, b);
        if (cd_remain_sec_ == 0) {
            cd_running_ = false;
            if (haptic_cb_) haptic_cb_(300);
            if (sw_btn_right_label_) lv_label_set_text(sw_btn_right_label_, "完成");
        }
    }

    // 闹钟检测
    if (alarm_enabled_) {
        time_t now = time(nullptr);
        struct tm ti;
        localtime_r(&now, &ti);
        if (ti.tm_hour == alarm_hour_ && ti.tm_min == alarm_min_ && ti.tm_sec == 0) {
            if (haptic_cb_) haptic_cb_(500);
        }
    }
}

void WatchPageManager::Update30ms() {
    if (current_page_ == WATCH_PAGE_STOPWATCH && sw_state_ == SW_RUNNING) {
        UpdateStopwatchUI();
    } else if (current_page_ == WATCH_PAGE_FLUIDBOX) {
        UpdateFluidBoxUI();
    } else if (current_page_ == WATCH_PAGE_FFT) {
        UpdateFftUI();
    } else if (current_page_ == WATCH_PAGE_LUCKY_WHEEL && wheel_spinning_) {
        UpdateLuckyWheelUI();
    }
}

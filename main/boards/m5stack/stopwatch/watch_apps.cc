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

static const char* WEEK_DAYS[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
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

    ESP_LOGI(TAG, "All Watch Apps (466x466) initialized successfully.");
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
// 1. WATCHFACE (4 款风格表盘，完全适配 466x466)
// -------------------------------------------------------------
void WatchPageManager::CreateWatchfaceScreen() {
    watchface_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(watchface_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_set_size(wf_container_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
    lv_obj_center(wf_container_);
    lv_obj_set_style_bg_opa(wf_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wf_container_, 0, 0);
    lv_obj_remove_flag(wf_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wf_container_, LV_OBJ_FLAG_EVENT_BUBBLE);

    wf_bat_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_bat_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_font(wf_bat_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wf_bat_label_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(wf_bat_label_, "BAT 100%");

    SwitchWatchfaceStyle(WF_STYLE_BIG_DIGITAL);
}

void WatchPageManager::SwitchWatchfaceStyle(WatchfaceStyle new_style) {
    wf_style_ = new_style;
    lv_obj_clean(wf_container_);

    wf_hour_label_ = nullptr;
    wf_min_label_ = nullptr;
    wf_date_label_ = nullptr;
    wf_sec_arc_ = nullptr;
    wf_hour_hand_ = nullptr;
    wf_min_hand_ = nullptr;
    wf_sec_hand_ = nullptr;
    wf_classic_date_label_ = nullptr;
    wf_classic_week_label_ = nullptr;
    wf_ring_hour_ = nullptr;
    wf_ring_min_ = nullptr;
    wf_ring_sec_ = nullptr;
    wf_sport_time_label_ = nullptr;
    wf_sport_date_label_ = nullptr;
    wf_zen_date_ = nullptr;
    wf_zen_time_ = nullptr;
    wf_zen_planet_ = nullptr;

    uint32_t color = THEME_COLORS[wf_theme_color_idx_];

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        // Style 0: Big Number (双层超大数字表盘)
        wf_sec_arc_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_sec_arc_, 450, 450);
        lv_obj_center(wf_sec_arc_);
        lv_arc_set_rotation(wf_sec_arc_, 270);
        lv_arc_set_bg_angles(wf_sec_arc_, 0, 360);
        lv_arc_set_range(wf_sec_arc_, 0, 60);
        lv_obj_remove_style(wf_sec_arc_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_sec_arc_, 5, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_sec_arc_, 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(color), LV_PART_INDICATOR);
        lv_obj_remove_flag(wf_sec_arc_, LV_OBJ_FLAG_CLICKABLE);

        wf_hour_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_hour_label_, LV_ALIGN_CENTER, 0, -68);
        lv_obj_set_style_text_font(wf_hour_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(wf_hour_label_, lv_color_hex(0xffffff), 0);

        wf_min_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_min_label_, LV_ALIGN_CENTER, 0, 48);
        lv_obj_set_style_text_font(wf_min_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(wf_min_label_, lv_color_hex(color), 0);

        wf_date_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_date_label_, LV_ALIGN_CENTER, 0, 130);
        lv_obj_set_style_text_font(wf_date_label_, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(wf_date_label_, lv_color_hex(0x94a3b8), 0);

    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        // Style 1: Classic (精密机械刻度表盘，圆心 233, 233)
        lv_obj_t* dial = lv_obj_create(wf_container_);
        lv_obj_set_size(dial, 450, 450);
        lv_obj_center(dial);
        lv_obj_set_style_bg_opa(dial, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(dial, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(dial, 2, 0);
        lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(dial, LV_OBJ_FLAG_SCROLLABLE);

        // 60 个外圈刻度
        for (int i = 0; i < 60; ++i) {
            float angle = i * 6.0f * 3.14159265f / 180.0f;
            bool is_hour = (i % 5 == 0);
            int outer_r = 216;
            int tick_len = is_hour ? 26 : 12;
            int inner_r = outer_r - tick_len;

            lv_obj_t* tick = lv_line_create(wf_container_);
            lv_point_precise_t pts[2];
            pts[0] = {(int32_t)(WATCH_CENTER_X + inner_r * sinf(angle)),
                      (int32_t)(WATCH_CENTER_Y - inner_r * cosf(angle))};
            pts[1] = {(int32_t)(WATCH_CENTER_X + outer_r * sinf(angle)),
                      (int32_t)(WATCH_CENTER_Y - outer_r * cosf(angle))};
            lv_line_set_points(tick, pts, 2);
            lv_obj_set_style_line_width(tick, is_hour ? 4 : 2, 0);
            lv_obj_set_style_line_color(tick, lv_color_hex(is_hour ? color : 0x475569), 0);
            lv_obj_remove_flag(tick, LV_OBJ_FLAG_CLICKABLE);
        }

        // 3 点钟日期窗口
        lv_obj_t* d_box = lv_obj_create(wf_container_);
        lv_obj_set_size(d_box, 48, 36);
        lv_obj_align(d_box, LV_ALIGN_CENTER, 125, 0);
        lv_obj_set_style_bg_color(d_box, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_border_color(d_box, lv_color_hex(0x475569), 0);
        lv_obj_set_style_border_width(d_box, 1, 0);
        lv_obj_set_style_radius(d_box, 8, 0);
        wf_classic_date_label_ = lv_label_create(d_box);
        lv_obj_center(wf_classic_date_label_);
        lv_obj_set_style_text_font(wf_classic_date_label_, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(wf_classic_date_label_, lv_color_hex(0xffffff), 0);

        // 12 点钟下方星期
        wf_classic_week_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_classic_week_label_, LV_ALIGN_CENTER, 0, -100);
        lv_obj_set_style_text_font(wf_classic_week_label_, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(wf_classic_week_label_, lv_color_hex(0x94a3b8), 0);

        // 时针、分针、秒针
        wf_hour_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_hour_hand_, 8, 0);
        lv_obj_set_style_line_color(wf_hour_hand_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_line_rounded(wf_hour_hand_, true, 0);

        wf_min_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_min_hand_, 5, 0);
        lv_obj_set_style_line_color(wf_min_hand_, lv_color_hex(0xe2e8f0), 0);
        lv_obj_set_style_line_rounded(wf_min_hand_, true, 0);

        wf_sec_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_sec_hand_, 3, 0);
        lv_obj_set_style_line_color(wf_sec_hand_, lv_color_hex(color), 0);
        lv_obj_set_style_line_rounded(wf_sec_hand_, true, 0);

        lv_obj_t* center_dot = lv_obj_create(wf_container_);
        lv_obj_set_size(center_dot, 16, 16);
        lv_obj_center(center_dot);
        lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(center_dot, 0, 0);
        lv_obj_set_style_bg_color(center_dot, lv_color_hex(color), 0);

    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        // Style 2: Sport Ring (运动三环健康表盘)
        wf_ring_hour_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_hour_, 430, 430);
        lv_obj_center(wf_ring_hour_);
        lv_arc_set_rotation(wf_ring_hour_, 270);
        lv_arc_set_bg_angles(wf_ring_hour_, 0, 360);
        lv_arc_set_range(wf_ring_hour_, 0, 24);
        lv_obj_remove_style(wf_ring_hour_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_hour_, 16, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_hour_, 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0xf43f5e), LV_PART_INDICATOR);

        wf_ring_min_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_min_, 376, 376);
        lv_obj_center(wf_ring_min_);
        lv_arc_set_rotation(wf_ring_min_, 270);
        lv_arc_set_bg_angles(wf_ring_min_, 0, 360);
        lv_arc_set_range(wf_ring_min_, 0, 60);
        lv_obj_remove_style(wf_ring_min_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_min_, 16, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_min_, 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x10b981), LV_PART_INDICATOR);

        wf_ring_sec_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_sec_, 322, 322);
        lv_obj_center(wf_ring_sec_);
        lv_arc_set_rotation(wf_ring_sec_, 270);
        lv_arc_set_bg_angles(wf_ring_sec_, 0, 360);
        lv_arc_set_range(wf_ring_sec_, 0, 60);
        lv_obj_remove_style(wf_ring_sec_, NULL, LV_PART_KNOB);
        lv_obj_set_style_arc_width(wf_ring_sec_, 16, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_sec_, 16, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

        wf_sport_time_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_sport_time_label_, LV_ALIGN_CENTER, 0, -15);
        lv_obj_set_style_text_font(wf_sport_time_label_, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(wf_sport_time_label_, lv_color_hex(0xffffff), 0);

        wf_sport_date_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_sport_date_label_, LV_ALIGN_CENTER, 0, 40);
        lv_obj_set_style_text_font(wf_sport_date_label_, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(wf_sport_date_label_, lv_color_hex(0x94a3b8), 0);

    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        // Style 3: Minimal Zen Planet (极简轨道行星表盘)
        lv_obj_t* orbit = lv_obj_create(wf_container_);
        lv_obj_set_size(orbit, 428, 428);
        lv_obj_center(orbit);
        lv_obj_set_style_bg_opa(orbit, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(orbit, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(orbit, 2, 0);
        lv_obj_set_style_radius(orbit, LV_RADIUS_CIRCLE, 0);
        lv_obj_remove_flag(orbit, LV_OBJ_FLAG_SCROLLABLE);

        wf_zen_planet_ = lv_obj_create(wf_container_);
        lv_obj_set_size(wf_zen_planet_, 18, 18);
        lv_obj_set_style_radius(wf_zen_planet_, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(wf_zen_planet_, 0, 0);
        lv_obj_set_style_bg_color(wf_zen_planet_, lv_color_hex(color), 0);

        wf_zen_time_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_time_, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_text_font(wf_zen_time_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(wf_zen_time_, lv_color_hex(0xf8fafc), 0);

        wf_zen_date_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_date_, LV_ALIGN_CENTER, 0, 60);
        lv_obj_set_style_text_font(wf_zen_date_, &lv_font_montserrat_20, 0);
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

    char hour_str[8], min_str[8], date_str[32], full_time[32];
    snprintf(hour_str, sizeof(hour_str), "%02d", ti.tm_hour);
    snprintf(min_str, sizeof(min_str), "%02d", ti.tm_min);
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d  %s",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, WEEK_DAYS[ti.tm_wday]);
    snprintf(full_time, sizeof(full_time), "%02d:%02d", ti.tm_hour, ti.tm_min);

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        if (wf_hour_label_) lv_label_set_text(wf_hour_label_, hour_str);
        if (wf_min_label_)  lv_label_set_text(wf_min_label_, min_str);
        if (wf_date_label_) lv_label_set_text(wf_date_label_, date_str);
        if (wf_sec_arc_)    lv_arc_set_value(wf_sec_arc_, ti.tm_sec);
    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        float h_angle = (ti.tm_hour % 12 + ti.tm_min / 60.0f) * 30.0f * 3.14159265f / 180.0f;
        float m_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f * 3.14159265f / 180.0f;
        float s_angle = (ti.tm_sec) * 6.0f * 3.14159265f / 180.0f;

        hour_pts_[0] = {WATCH_CENTER_X, WATCH_CENTER_Y};
        hour_pts_[1] = {(int32_t)(WATCH_CENTER_X + 110 * sinf(h_angle)),
                        (int32_t)(WATCH_CENTER_Y - 110 * cosf(h_angle))};
        min_pts_[0]  = {WATCH_CENTER_X, WATCH_CENTER_Y};
        min_pts_[1]  = {(int32_t)(WATCH_CENTER_X + 160 * sinf(m_angle)),
                        (int32_t)(WATCH_CENTER_Y - 160 * cosf(m_angle))};
        sec_pts_[0]  = {WATCH_CENTER_X, WATCH_CENTER_Y};
        sec_pts_[1]  = {(int32_t)(WATCH_CENTER_X + 195 * sinf(s_angle)),
                        (int32_t)(WATCH_CENTER_Y - 195 * cosf(s_angle))};

        if (wf_hour_hand_) lv_line_set_points(wf_hour_hand_, hour_pts_, 2);
        if (wf_min_hand_)  lv_line_set_points(wf_min_hand_, min_pts_, 2);
        if (wf_sec_hand_)  lv_line_set_points(wf_sec_hand_, sec_pts_, 2);

        if (wf_classic_date_label_) {
            char d_b[8];
            snprintf(d_b, sizeof(d_b), "%02d", ti.tm_mday);
            lv_label_set_text(wf_classic_date_label_, d_b);
        }
        if (wf_classic_week_label_) {
            lv_label_set_text(wf_classic_week_label_, WEEK_DAYS[ti.tm_wday]);
        }
    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        if (wf_ring_hour_) lv_arc_set_value(wf_ring_hour_, ti.tm_hour);
        if (wf_ring_min_)  lv_arc_set_value(wf_ring_min_, ti.tm_min);
        if (wf_ring_sec_)  lv_arc_set_value(wf_ring_sec_, ti.tm_sec);
        if (wf_sport_time_label_) lv_label_set_text(wf_sport_time_label_, full_time);
        if (wf_sport_date_label_) lv_label_set_text(wf_sport_date_label_, date_str);
    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        if (wf_zen_time_) lv_label_set_text(wf_zen_time_, full_time);
        if (wf_zen_date_) lv_label_set_text(wf_zen_date_, date_str);
        if (wf_zen_planet_) {
            float s_angle = (ti.tm_sec) * 6.0f * 3.14159265f / 180.0f;
            int px = (int)(WATCH_CENTER_X + 214 * sinf(s_angle)) - 9;
            int py = (int)(WATCH_CENTER_Y - 214 * cosf(s_angle)) - 9;
            lv_obj_set_pos(wf_zen_planet_, px, py);
        }
    }
}

// -------------------------------------------------------------
// 2. STOPWATCH & COUNTDOWN TIMER (466x466 全屏适配)
// -------------------------------------------------------------
void WatchPageManager::CreateStopwatchScreen() {
    stopwatch_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(stopwatch_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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

    // 顶部模式切换胶囊
    sw_mode_tab_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_mode_tab_, 180, 40);
    lv_obj_align(sw_mode_tab_, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_radius(sw_mode_tab_, 20, 0);
    lv_obj_set_style_bg_color(sw_mode_tab_, lv_color_hex(0x1e293b), 0);
    lv_obj_t* tab_txt = lv_label_create(sw_mode_tab_);
    lv_obj_center(tab_txt);
    lv_obj_set_style_text_font(tab_txt, &lv_font_montserrat_16, 0);
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

    // 420px 巨型外环跑马刻度
    sw_arc_ = lv_arc_create(stopwatch_screen_);
    lv_obj_set_size(sw_arc_, 420, 420);
    lv_obj_center(sw_arc_);
    lv_arc_set_rotation(sw_arc_, 270);
    lv_arc_set_bg_angles(sw_arc_, 0, 360);
    lv_arc_set_range(sw_arc_, 0, 60000);
    lv_obj_remove_style(sw_arc_, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(sw_arc_, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(sw_arc_, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

    sw_time_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_time_label_, LV_ALIGN_CENTER, -30, -36);
    lv_obj_set_style_text_font(sw_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(sw_time_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(sw_time_label_, "00:00");

    sw_ms_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_ms_label_, LV_ALIGN_CENTER, 96, -30);
    lv_obj_set_style_text_font(sw_ms_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sw_ms_label_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(sw_ms_label_, ".00");

    sw_laps_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_laps_label_, LV_ALIGN_CENTER, 0, 45);
    lv_obj_set_style_text_font(sw_laps_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sw_laps_label_, lv_color_hex(0x94a3b8), 0);
    lv_label_set_text(sw_laps_label_, "分段计圈就绪");

    // 大尺寸触控按键
    sw_btn_left_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_left_, 130, 54);
    lv_obj_align(sw_btn_left_, LV_ALIGN_BOTTOM_LEFT, 64, -60);
    lv_obj_set_style_radius(sw_btn_left_, 27, 0);
    lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x334155), 0);
    sw_btn_left_label_ = lv_label_create(sw_btn_left_);
    lv_obj_center(sw_btn_left_label_);
    lv_obj_set_style_text_font(sw_btn_left_label_, &lv_font_montserrat_18, 0);
    lv_label_set_text(sw_btn_left_label_, "复位");

    lv_obj_add_event_cb(sw_btn_left_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (self->haptic_cb_) self->haptic_cb_(30);
        if (self->sw_mode_ == MODE_COUNTDOWN) {
            if (self->cd_total_sec_ == 60) self->cd_total_sec_ = 180;
            else if (self->cd_total_sec_ == 180) self->cd_total_sec_ = 300;
            else if (self->cd_total_sec_ == 300) self->cd_total_sec_ = 600;
            else self->cd_total_sec_ = 60;
            self->cd_remain_sec_ = self->cd_total_sec_;
            char b[16];
            snprintf(b, sizeof(b), "%dm", (int)(self->cd_total_sec_ / 60));
            lv_label_set_text(self->sw_btn_left_label_, b);
            snprintf(b, sizeof(b), "%02d:00", (int)(self->cd_total_sec_ / 60));
            lv_label_set_text(self->sw_time_label_, b);
        } else {
            if (self->sw_state_ == SW_RUNNING) {
                if (self->sw_laps_.size() < 10) {
                    self->sw_laps_.push_back(self->sw_elapsed_ms_);
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Lap %d: %02d:%02d.%02d",
                             (int)self->sw_laps_.size(),
                             (int)((self->sw_elapsed_ms_ / 1000) / 60),
                             (int)((self->sw_elapsed_ms_ / 1000) % 60),
                             (int)((self->sw_elapsed_ms_ % 1000) / 10));
                    lv_label_set_text(self->sw_laps_label_, buf);
                }
            } else {
                self->sw_state_ = SW_STOPPED;
                self->sw_elapsed_ms_ = 0;
                self->sw_laps_.clear();
                lv_label_set_text(self->sw_time_label_, "00:00");
                lv_label_set_text(self->sw_ms_label_, ".00");
                lv_arc_set_value(self->sw_arc_, 0);
                lv_label_set_text(self->sw_laps_label_, "分段计圈就绪");
                lv_label_set_text(self->sw_btn_left_label_, "复位");
                lv_label_set_text(self->sw_btn_right_label_, "启动");
            }
        }
    }, LV_EVENT_CLICKED, this);

    sw_btn_right_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_right_, 130, 54);
    lv_obj_align(sw_btn_right_, LV_ALIGN_BOTTOM_RIGHT, -64, -60);
    lv_obj_set_style_radius(sw_btn_right_, 27, 0);
    lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x059669), 0);
    sw_btn_right_label_ = lv_label_create(sw_btn_right_);
    lv_obj_center(sw_btn_right_label_);
    lv_obj_set_style_text_font(sw_btn_right_label_, &lv_font_montserrat_18, 0);
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
        snprintf(time_b, sizeof(time_b), "%02u:%02u", (unsigned int)min, (unsigned int)sec);
        snprintf(ms_b, sizeof(ms_b), ".%02u", (unsigned int)ms);

        if (sw_time_label_) lv_label_set_text(sw_time_label_, time_b);
        if (sw_ms_label_)   lv_label_set_text(sw_ms_label_, ms_b);
        if (sw_arc_)        lv_arc_set_value(sw_arc_, sw_elapsed_ms_ % 60000);
    }
}

// -------------------------------------------------------------
// 3. ALARM (定时闹钟，466x466 全屏适配)
// -------------------------------------------------------------
void WatchPageManager::CreateAlarmScreen() {
    alarm_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(alarm_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "定时闹钟");

    alarm_time_label_ = lv_label_create(alarm_screen_);
    lv_obj_align(alarm_time_label_, LV_ALIGN_CENTER, 0, -35);
    lv_obj_set_style_text_font(alarm_time_label_, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(alarm_time_label_, lv_color_hex(0x38bdf8), 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarm_hour_, alarm_min_);
    lv_label_set_text(alarm_time_label_, buf);

    // 4 个大微调触控按键
    lv_obj_t* btn_h_plus = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_h_plus, 86, 48);
    lv_obj_align(btn_h_plus, LV_ALIGN_CENTER, -85, 45);
    lv_obj_set_style_radius(btn_h_plus, 24, 0);
    lv_obj_set_style_bg_color(btn_h_plus, lv_color_hex(0x1e293b), 0);
    lv_obj_t* t_hp = lv_label_create(btn_h_plus);
    lv_obj_center(t_hp);
    lv_obj_set_style_text_font(t_hp, &lv_font_montserrat_16, 0);
    lv_label_set_text(t_hp, "+ 时");
    lv_obj_add_event_cb(btn_h_plus, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_hour_ = (self->alarm_hour_ + 1) % 24;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_h_min = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_h_min, 86, 48);
    lv_obj_align(btn_h_min, LV_ALIGN_CENTER, -85, 105);
    lv_obj_set_style_radius(btn_h_min, 24, 0);
    lv_obj_set_style_bg_color(btn_h_min, lv_color_hex(0x0f172a), 0);
    lv_obj_t* t_hm = lv_label_create(btn_h_min);
    lv_obj_center(t_hm);
    lv_obj_set_style_text_font(t_hm, &lv_font_montserrat_16, 0);
    lv_label_set_text(t_hm, "- 时");
    lv_obj_add_event_cb(btn_h_min, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_hour_ = (self->alarm_hour_ + 23) % 24;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_m_plus = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_m_plus, 86, 48);
    lv_obj_align(btn_m_plus, LV_ALIGN_CENTER, 85, 45);
    lv_obj_set_style_radius(btn_m_plus, 24, 0);
    lv_obj_set_style_bg_color(btn_m_plus, lv_color_hex(0x1e293b), 0);
    lv_obj_t* t_mp = lv_label_create(btn_m_plus);
    lv_obj_center(t_mp);
    lv_obj_set_style_text_font(t_mp, &lv_font_montserrat_16, 0);
    lv_label_set_text(t_mp, "+ 分");
    lv_obj_add_event_cb(btn_m_plus, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_min_ = (self->alarm_min_ + 5) % 60;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_m_min = lv_btn_create(alarm_screen_);
    lv_obj_set_size(btn_m_min, 86, 48);
    lv_obj_align(btn_m_min, LV_ALIGN_CENTER, 85, 105);
    lv_obj_set_style_radius(btn_m_min, 24, 0);
    lv_obj_set_style_bg_color(btn_m_min, lv_color_hex(0x0f172a), 0);
    lv_obj_t* t_mm = lv_label_create(btn_m_min);
    lv_obj_center(t_mm);
    lv_obj_set_style_text_font(t_mm, &lv_font_montserrat_16, 0);
    lv_label_set_text(t_mm, "- 分");
    lv_obj_add_event_cb(btn_m_min, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_min_ = (self->alarm_min_ + 55) % 60;
            self->UpdateAlarmUI();
            if (self->haptic_cb_) self->haptic_cb_(20);
        }
    }, LV_EVENT_CLICKED, this);

    // 闹钟开关
    alarm_switch_ = lv_switch_create(alarm_screen_);
    lv_obj_align(alarm_switch_, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_size(alarm_switch_, 80, 40);
    lv_obj_add_event_cb(alarm_switch_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self) {
            self->alarm_enabled_ = lv_obj_has_state(self->alarm_switch_, LV_STATE_CHECKED);
            if (self->haptic_cb_) self->haptic_cb_(50);
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
// 4. FLUIDBOX (重力流体沙漏，466x466 全屏适配与真实物理)
// -------------------------------------------------------------
void WatchPageManager::CreateFluidBoxScreen() {
    fluidbox_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(fluidbox_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_set_size(fluid_ring_, 430, 430);
    lv_obj_center(fluid_ring_);
    lv_obj_set_style_bg_opa(fluid_ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(fluid_ring_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(fluid_ring_, 3, 0);
    lv_obj_set_style_radius(fluid_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(fluid_ring_, LV_OBJ_FLAG_SCROLLABLE);

    particles_.clear();
    const uint32_t colors[] = {0x38bdf8, 0x06b6d4, 0x3b82f6, 0x60a5fa, 0x93c5fd};
    for (int i = 0; i < FLUID_PARTICLE_COUNT; ++i) {
        FluidParticle p;
        p.x = (float)(WATCH_CENTER_X - 60 + (esp_random() % 120));
        p.y = (float)(WATCH_CENTER_Y - 60 + (esp_random() % 120));
        p.vx = 0.0f;
        p.vy = 0.0f;
        p.color = colors[i % 5];
        p.obj = lv_obj_create(fluidbox_screen_);
        lv_obj_set_size(p.obj, 14, 14);
        lv_obj_set_pos(p.obj, (int)p.x, (int)p.y);
        lv_obj_set_style_radius(p.obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(p.obj, 0, 0);
        lv_obj_set_style_bg_color(p.obj, lv_color_hex(p.color), 0);
        lv_obj_remove_flag(p.obj, LV_OBJ_FLAG_CLICKABLE);
        particles_.push_back(p);
    }

    lv_obj_t* hint = lv_label_create(fluidbox_screen_);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748b), 0);
    lv_label_set_text(hint, "倾斜手表感受重力流体");
}

void WatchPageManager::UpdateFluidBoxUI() {
    const float cx = (float)WATCH_CENTER_X;
    const float cy = (float)WATCH_CENTER_Y;
    const float max_r = 202.0f;

    for (auto& p : particles_) {
        p.vx += imu_gx_ * 0.18f;
        p.vy += imu_gy_ * 0.18f;
        p.vx *= 0.96f;
        p.vy *= 0.96f;

        p.x += p.vx;
        p.y += p.vy;

        float dx = p.x - cx;
        float dy = p.y - cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > max_r) {
            float nx = dx / dist;
            float ny = dy / dist;
            p.x = cx + nx * max_r;
            p.y = cy + ny * max_r;

            float dot = p.vx * nx + p.vy * ny;
            p.vx = (p.vx - 1.6f * dot * nx) * 0.65f;
            p.vy = (p.vy - 1.6f * dot * ny) * 0.65f;
        }

        if (p.obj) {
            lv_obj_set_pos(p.obj, (int)(p.x - 7), (int)(p.y - 7));
        }
    }
}

// -------------------------------------------------------------
// 5. AUDIO FFT SPECTRUM (466x466 环形放射状立体频谱)
// -------------------------------------------------------------
void WatchPageManager::CreateFftScreen() {
    fft_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(fft_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "音频频谱");

    // 黑胶大碟盘
    lv_obj_t* disc = lv_obj_create(fft_screen_);
    lv_obj_set_size(disc, 150, 150);
    lv_obj_center(disc);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0x18181b), 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(0xa855f7), 0);
    lv_obj_set_style_border_width(disc, 3, 0);

    lv_obj_t* disc_lbl = lv_label_create(disc);
    lv_obj_center(disc_lbl);
    lv_obj_set_style_text_font(disc_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(disc_lbl, lv_color_hex(0xa855f7), 0);
    lv_label_set_text(disc_lbl, "MUSE BEAT");

    // 32 根发散跳动频柱
    const int start_x = 42;
    const int gap = 12;
    for (int i = 0; i < FFT_BAR_COUNT; ++i) {
        fft_bars_[i] = lv_obj_create(fft_screen_);
        lv_obj_set_size(fft_bars_[i], 9, 30);
        lv_obj_set_pos(fft_bars_[i], start_x + i * gap, 290);
        lv_obj_set_style_radius(fft_bars_[i], 4, 0);
        lv_obj_set_style_border_width(fft_bars_[i], 0, 0);
        uint32_t col = (i < 10) ? 0x10b981 : (i < 22 ? 0x38bdf8 : 0xf43f5e);
        lv_obj_set_style_bg_color(fft_bars_[i], lv_color_hex(col), 0);
    }
}

void WatchPageManager::UpdateFftUI() {
    for (int i = 0; i < FFT_BAR_COUNT; ++i) {
        if (!fft_bars_[i]) continue;
        int target_h = 15 + (esp_random() % 140);
        fft_heights_[i] = (int)(fft_heights_[i] * 0.6f + target_h * 0.4f);
        lv_obj_set_size(fft_bars_[i], 9, fft_heights_[i]);
        lv_obj_set_y(fft_bars_[i], 320 - fft_heights_[i]);
    }
}

// -------------------------------------------------------------
// 6. LUCKY WHEEL (幸运大转盘，466x466 全屏适配)
// -------------------------------------------------------------
void WatchPageManager::CreateLuckyWheelScreen() {
    lucky_wheel_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(lucky_wheel_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_set_size(wheel_arc_, 420, 420);
    lv_obj_center(wheel_arc_);
    lv_arc_set_rotation(wheel_arc_, 0);
    lv_arc_set_bg_angles(wheel_arc_, 0, 360);
    lv_arc_set_range(wheel_arc_, 0, 360);
    lv_obj_remove_style(wheel_arc_, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(wheel_arc_, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(wheel_arc_, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0xf59e0b), LV_PART_INDICATOR);

    wheel_result_label_ = lv_label_create(lucky_wheel_screen_);
    lv_obj_align(wheel_result_label_, LV_ALIGN_CENTER, 0, -60);
    lv_obj_set_style_text_font(wheel_result_label_, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(wheel_result_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(wheel_result_label_, "做个决定？");

    wheel_spin_btn_ = lv_btn_create(lucky_wheel_screen_);
    lv_obj_set_size(wheel_spin_btn_, 180, 56);
    lv_obj_align(wheel_spin_btn_, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_radius(wheel_spin_btn_, 28, 0);
    lv_obj_set_style_bg_color(wheel_spin_btn_, lv_color_hex(0xd97706), 0);
    lv_obj_t* btn_lbl = lv_label_create(wheel_spin_btn_);
    lv_obj_center(btn_lbl);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(btn_lbl, "SPIN 开始！");

    lv_obj_add_event_cb(wheel_spin_btn_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && !self->wheel_spinning_) {
            self->wheel_spinning_ = true;
            self->wheel_speed_ = 40.0f + (esp_random() % 30);
            self->wheel_chosen_idx_ = esp_random() % 6;
            lv_label_set_text(self->wheel_result_label_, "旋转决策中...");
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
// 7. BADGE (极客电子工牌，466x466 全屏适配)
// -------------------------------------------------------------
void WatchPageManager::CreateBadgeScreen() {
    badge_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(badge_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
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
    lv_obj_set_size(badge_card_, 380, 360);
    lv_obj_center(badge_card_);
    lv_obj_set_style_bg_color(badge_card_, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_border_color(badge_card_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_border_width(badge_card_, 3, 0);
    lv_obj_set_style_radius(badge_card_, 36, 0);
    lv_obj_remove_flag(badge_card_, LV_OBJ_FLAG_SCROLLABLE);

    badge_name_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_name_label_, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_set_style_text_font(badge_name_label_, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(badge_name_label_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(badge_name_label_, BADGE_PROFILES[0].name);

    badge_title_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_title_label_, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_text_font(badge_title_label_, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(badge_title_label_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(badge_title_label_, BADGE_PROFILES[0].title);

    badge_desc_label_ = lv_label_create(badge_card_);
    lv_obj_align(badge_desc_label_, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_set_style_text_font(badge_desc_label_, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(badge_desc_label_, lv_color_hex(0x94a3b8), 0);
    lv_label_set_text(badge_desc_label_, BADGE_PROFILES[0].quote);

    lv_obj_t* hint = lv_label_create(badge_screen_);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x64748b), 0);
    lv_label_set_text(hint, "轻触名片切换身份");
}

// -------------------------------------------------------------
// 8. SETTINGS & CONTROL CENTER (466x466 全屏可滚动设置)
// -------------------------------------------------------------
void WatchPageManager::CreateSettingsScreen() {
    settings_screen_ = lv_obj_create(NULL);
    lv_obj_set_size(settings_screen_, WATCH_DISPLAY_WIDTH, WATCH_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(settings_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(settings_screen_, LV_OPA_COVER, 0);
    lv_obj_set_scroll_dir(settings_screen_, LV_DIR_VER);

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
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(title, &font_noto_sans_basic_30_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_label_set_text(title, "控制中心");

    // 亮度调节滑块 (宽 300)
    lv_obj_t* b_lbl = lv_label_create(settings_screen_);
    lv_obj_align(b_lbl, LV_ALIGN_TOP_LEFT, 50, 85);
    lv_obj_set_style_text_color(b_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(b_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(b_lbl, "屏幕亮度");

    bright_val_label_ = lv_label_create(settings_screen_);
    lv_obj_align(bright_val_label_, LV_ALIGN_TOP_RIGHT, -50, 85);
    lv_obj_set_style_text_color(bright_val_label_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_text_font(bright_val_label_, &lv_font_montserrat_16, 0);
    lv_label_set_text(bright_val_label_, "80%");

    bright_slider_ = lv_slider_create(settings_screen_);
    lv_obj_set_size(bright_slider_, 366, 24);
    lv_obj_align(bright_slider_, LV_ALIGN_TOP_MID, 0, 115);
    lv_slider_set_range(bright_slider_, 10, 100);
    lv_slider_set_value(bright_slider_, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(bright_slider_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->bright_cb_) {
            int val = lv_slider_get_value(self->bright_slider_);
            self->bright_cb_((uint8_t)val);
            char b[16];
            snprintf(b, sizeof(b), "%d%%", val);
            lv_label_set_text(self->bright_val_label_, b);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // 音量调节滑块 (宽 300)
    lv_obj_t* v_lbl = lv_label_create(settings_screen_);
    lv_obj_align(v_lbl, LV_ALIGN_TOP_LEFT, 50, 155);
    lv_obj_set_style_text_color(v_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(v_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(v_lbl, "扬声器音量");

    vol_val_label_ = lv_label_create(settings_screen_);
    lv_obj_align(vol_val_label_, LV_ALIGN_TOP_RIGHT, -50, 155);
    lv_obj_set_style_text_color(vol_val_label_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_text_font(vol_val_label_, &lv_font_montserrat_16, 0);
    lv_label_set_text(vol_val_label_, "70%");

    vol_slider_ = lv_slider_create(settings_screen_);
    lv_obj_set_size(vol_slider_, 366, 24);
    lv_obj_align(vol_slider_, LV_ALIGN_TOP_MID, 0, 185);
    lv_slider_set_range(vol_slider_, 0, 100);
    lv_slider_set_value(vol_slider_, 70, LV_ANIM_OFF);
    lv_obj_add_event_cb(vol_slider_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->vol_cb_) {
            int val = lv_slider_get_value(self->vol_slider_);
            self->vol_cb_((uint8_t)val);
            char b[16];
            snprintf(b, sizeof(b), "%d%%", val);
            lv_label_set_text(self->vol_val_label_, b);
        }
    }, LV_EVENT_VALUE_CHANGED, this);

    // 触觉震动测试
    lv_obj_t* haptic_btn = lv_btn_create(settings_screen_);
    lv_obj_set_size(haptic_btn, 170, 48);
    lv_obj_align(haptic_btn, LV_ALIGN_TOP_LEFT, 50, 230);
    lv_obj_set_style_radius(haptic_btn, 24, 0);
    lv_obj_set_style_bg_color(haptic_btn, lv_color_hex(0x1e293b), 0);
    lv_obj_t* hb_txt = lv_label_create(haptic_btn);
    lv_obj_center(hb_txt);
    lv_obj_set_style_text_font(hb_txt, &lv_font_montserrat_16, 0);
    lv_label_set_text(hb_txt, "马达震动测试");
    lv_obj_add_event_cb(haptic_btn, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (self && self->haptic_cb_) self->haptic_cb_(100);
    }, LV_EVENT_CLICKED, this);

    // 重启设备按键
    lv_obj_t* reboot_btn = lv_btn_create(settings_screen_);
    lv_obj_set_size(reboot_btn, 170, 48);
    lv_obj_align(reboot_btn, LV_ALIGN_TOP_RIGHT, -50, 230);
    lv_obj_set_style_radius(reboot_btn, 24, 0);
    lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0x450a0a), 0);
    lv_obj_t* rb_txt = lv_label_create(reboot_btn);
    lv_obj_center(rb_txt);
    lv_obj_set_style_text_font(rb_txt, &lv_font_montserrat_16, 0);
    lv_label_set_text(rb_txt, "系统重启");
    lv_obj_add_event_cb(reboot_btn, [](lv_event_t* e) {
        esp_restart();
    }, LV_EVENT_CLICKED, this);

    // 硬件与系统信息卡片
    lv_obj_t* info_card = lv_obj_create(settings_screen_);
    lv_obj_set_size(info_card, 366, 120);
    lv_obj_align(info_card, LV_ALIGN_TOP_MID, 0, 300);
    lv_obj_set_style_bg_color(info_card, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_border_color(info_card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(info_card, 1, 0);
    lv_obj_set_style_radius(info_card, 20, 0);

    dev_info_label_ = lv_label_create(info_card);
    lv_obj_center(dev_info_label_);
    lv_obj_set_style_text_color(dev_info_label_, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(dev_info_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(dev_info_label_,
                      "M5Stack StopWatch v2.5.5\n"
                      "RTC: RX8130 硬件在线\n"
                      "IMU: BMI270 六轴运动传感器就绪\n"
                      "ESP32-S3 Dual-Core 240MHz (8MB PSRAM)");
}

// -------------------------------------------------------------
// 定时循环刷新 (1s & 30ms)
// -------------------------------------------------------------
void WatchPageManager::Update1s() {
    if (current_page_ == WATCH_PAGE_WATCHFACE) {
        UpdateWatchfaceUI();
    }

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

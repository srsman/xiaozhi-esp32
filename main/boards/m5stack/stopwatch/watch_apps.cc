#include "watch_apps.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_random.h>
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

WatchPageManager& WatchPageManager::GetInstance() {
    static WatchPageManager instance;
    return instance;
}

void WatchPageManager::Init(lv_obj_t* muse_screen, HapticCallback haptic_cb, BatteryCallback bat_cb) {
    muse_screen_ = muse_screen;
    haptic_cb_ = haptic_cb;
    bat_cb_ = bat_cb;

    // Register gesture on muse_screen to swipe into watchface
    if (muse_screen_) {
        lv_obj_add_event_cb(muse_screen_, [](lv_event_t* e) {
            auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
            if (!self) return;
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            }
        }, LV_EVENT_GESTURE, this);
    }

    CreateWatchfaceScreen();
    CreateStopwatchScreen();
    CreateLuckyWheelScreen();

    // Background timers for UI updates
    lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<WatchPageManager*>(lv_timer_get_user_data(t));
        if (self) self->Update1s();
    }, 1000, this);

    lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<WatchPageManager*>(lv_timer_get_user_data(t));
        if (self) self->Update30ms();
    }, 30, this);
}

void WatchPageManager::SwitchToPage(WatchPageId page_id, lv_screen_load_anim_t anim) {
    current_page_ = page_id;
    lv_obj_t* target = nullptr;
    switch (page_id) {
        case WATCH_PAGE_MUSE:
            target = muse_screen_;
            break;
        case WATCH_PAGE_WATCHFACE:
            target = watchface_screen_;
            UpdateWatchfaceUI();
            break;
        case WATCH_PAGE_STOPWATCH:
            target = stopwatch_screen_;
            UpdateStopwatchUI();
            break;
        case WATCH_PAGE_LUCKY_WHEEL:
            target = lucky_wheel_screen_;
            UpdateLuckyWheelUI();
            break;
        default:
            target = muse_screen_;
            break;
    }
    if (target != nullptr) {
        lv_screen_load_anim(target, anim, 220, 0, false);
    }
}

bool WatchPageManager::HandleBack() {
    if (current_page_ == WATCH_PAGE_LUCKY_WHEEL) {
        if (haptic_cb_) haptic_cb_(30);
        SwitchToPage(WATCH_PAGE_STOPWATCH, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        return true;
    } else if (current_page_ == WATCH_PAGE_STOPWATCH) {
        if (haptic_cb_) haptic_cb_(30);
        SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        return true;
    } else if (current_page_ == WATCH_PAGE_WATCHFACE) {
        if (haptic_cb_) haptic_cb_(30);
        SwitchToPage(WATCH_PAGE_MUSE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        return true;
    }
    return false;
}

// -------------------------------------------------------------
// 1. WATCHFACE (4 款风格表盘)
// -------------------------------------------------------------
void WatchPageManager::CreateWatchfaceScreen() {
    watchface_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(watchface_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(watchface_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(watchface_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Gestures: swipe right -> Muse, swipe left -> Stopwatch
    // Long press / click -> Switch watchface style or color theme
    lv_obj_add_event_cb(watchface_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_MUSE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_STOPWATCH, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            }
        } else if (code == LV_EVENT_LONG_PRESSED) {
            // Long press: switch watchface style (Digital -> Analog -> Sport -> Zen)
            self->SwitchWatchfaceStyle((WatchfaceStyle)((self->wf_style_ + 1) % WF_STYLE_MAX));
            if (self->haptic_cb_) self->haptic_cb_(60);
        } else if (code == LV_EVENT_CLICKED) {
            // Click: switch theme color
            self->wf_theme_color_idx_ = (self->wf_theme_color_idx_ + 1) % 4;
            if (self->haptic_cb_) self->haptic_cb_(30);
            self->UpdateWatchfaceUI();
        }
    }, LV_EVENT_ALL, this);

    // Top battery info
    wf_bat_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_bat_label_, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_color(wf_bat_label_, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(wf_bat_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(wf_bat_label_, "100%");

    // Dynamic style container
    wf_container_ = lv_obj_create(watchface_screen_);
    lv_obj_set_size(wf_container_, 280, 280);
    lv_obj_center(wf_container_);
    lv_obj_set_style_bg_opa(wf_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wf_container_, 0, 0);
    lv_obj_remove_flag(wf_container_, LV_OBJ_FLAG_SCROLLABLE);

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
    wf_zen_time_ = nullptr;
    wf_zen_date_ = nullptr;

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        // Outer second arc
        wf_sec_arc_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_sec_arc_, 272, 272);
        lv_obj_center(wf_sec_arc_);
        lv_arc_set_rotation(wf_sec_arc_, 270);
        lv_arc_set_bg_angles(wf_sec_arc_, 0, 360);
        lv_arc_set_range(wf_sec_arc_, 0, 60);
        lv_obj_remove_style(wf_sec_arc_, NULL, LV_PART_KNOB);
        lv_obj_remove_flag(wf_sec_arc_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(wf_sec_arc_, 4, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_sec_arc_, 5, LV_PART_INDICATOR);

        // Center Big Time (HH:MM)
        wf_time_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_time_label_, LV_ALIGN_CENTER, -12, -8);
        lv_obj_set_style_text_color(wf_time_label_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(wf_time_label_, &lv_font_montserrat_48, 0);

        // Second badge
        wf_sec_label_ = lv_label_create(wf_container_);
        lv_obj_align_to(wf_sec_label_, wf_time_label_, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
        lv_obj_set_style_text_font(wf_sec_label_, &lv_font_montserrat_18, 0);

        // Date & Weekday label
        wf_date_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_date_label_, LV_ALIGN_CENTER, 0, 46);
        lv_obj_set_style_text_color(wf_date_label_, lv_color_hex(0xa0aec0), 0);
        lv_obj_set_style_text_font(wf_date_label_, &lv_font_montserrat_16, 0);
    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        // Dial circle with hour marks
        lv_obj_t* dial = lv_arc_create(wf_container_);
        lv_obj_set_size(dial, 270, 270);
        lv_obj_center(dial);
        lv_arc_set_rotation(dial, 0);
        lv_arc_set_bg_angles(dial, 0, 360);
        lv_obj_remove_style(dial, NULL, LV_PART_KNOB);
        lv_obj_remove_style(dial, NULL, LV_PART_INDICATOR);
        lv_obj_remove_flag(dial, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(dial, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_color(dial, lv_color_hex(0x334155), LV_PART_MAIN);

        // Hands (Lines)
        wf_hour_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_hour_hand_, 6, 0);
        lv_obj_set_style_line_rounded(wf_hour_hand_, true, 0);
        lv_obj_set_style_line_color(wf_hour_hand_, lv_color_hex(0xffffff), 0);

        wf_min_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_min_hand_, 4, 0);
        lv_obj_set_style_line_rounded(wf_min_hand_, true, 0);
        lv_obj_set_style_line_color(wf_min_hand_, lv_color_hex(0x94a3b8), 0);

        wf_sec_hand_ = lv_line_create(wf_container_);
        lv_obj_set_style_line_width(wf_sec_hand_, 2, 0);
        lv_obj_set_style_line_color(wf_sec_hand_, lv_color_hex(0xef4444), 0);

        // Center hub dot
        lv_obj_t* hub = lv_obj_create(wf_container_);
        lv_obj_set_size(hub, 12, 12);
        lv_obj_center(hub);
        lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(hub, lv_color_hex(0xef4444), 0);
        lv_obj_set_style_border_width(hub, 0, 0);

        // Date text
        wf_date_label_ = lv_label_create(wf_container_);
        lv_obj_align(wf_date_label_, LV_ALIGN_CENTER, 0, 60);
        lv_obj_set_style_text_color(wf_date_label_, lv_color_hex(0x64748b), 0);
        lv_obj_set_style_text_font(wf_date_label_, &lv_font_montserrat_14, 0);
    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        // Apple Watch style 3 concentric rings
        wf_ring_sec_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_sec_, 260, 260);
        lv_obj_center(wf_ring_sec_);
        lv_arc_set_rotation(wf_ring_sec_, 270);
        lv_arc_set_bg_angles(wf_ring_sec_, 0, 360);
        lv_arc_set_range(wf_ring_sec_, 0, 60);
        lv_obj_remove_style(wf_ring_sec_, NULL, LV_PART_KNOB);
        lv_obj_remove_flag(wf_ring_sec_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(wf_ring_sec_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_sec_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_sec_, lv_color_hex(0xef4444), LV_PART_INDICATOR);

        wf_ring_min_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_min_, 234, 234);
        lv_obj_center(wf_ring_min_);
        lv_arc_set_rotation(wf_ring_min_, 270);
        lv_arc_set_bg_angles(wf_ring_min_, 0, 360);
        lv_arc_set_range(wf_ring_min_, 0, 60);
        lv_obj_remove_style(wf_ring_min_, NULL, LV_PART_KNOB);
        lv_obj_remove_flag(wf_ring_min_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(wf_ring_min_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_min_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_min_, lv_color_hex(0x10b981), LV_PART_INDICATOR);

        wf_ring_hour_ = lv_arc_create(wf_container_);
        lv_obj_set_size(wf_ring_hour_, 208, 208);
        lv_obj_center(wf_ring_hour_);
        lv_arc_set_rotation(wf_ring_hour_, 270);
        lv_arc_set_bg_angles(wf_ring_hour_, 0, 360);
        lv_arc_set_range(wf_ring_hour_, 0, 24);
        lv_obj_remove_style(wf_ring_hour_, NULL, LV_PART_KNOB);
        lv_obj_remove_flag(wf_ring_hour_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(wf_ring_hour_, 10, LV_PART_MAIN);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0x1e293b), LV_PART_MAIN);
        lv_obj_set_style_arc_width(wf_ring_hour_, 10, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(wf_ring_hour_, lv_color_hex(0x06b6d4), LV_PART_INDICATOR);

        wf_sport_time_label_ = lv_label_create(wf_container_);
        lv_obj_center(wf_sport_time_label_);
        lv_obj_set_style_text_color(wf_sport_time_label_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(wf_sport_time_label_, &lv_font_montserrat_20, 0);
    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        // Minimalist Zen clock
        wf_zen_time_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_time_, LV_ALIGN_CENTER, 0, -18);
        lv_obj_set_style_text_color(wf_zen_time_, lv_color_hex(0xf8fafc), 0);
        lv_obj_set_style_text_font(wf_zen_time_, &lv_font_montserrat_40, 0);

        wf_zen_date_ = lv_label_create(wf_container_);
        lv_obj_align(wf_zen_date_, LV_ALIGN_CENTER, 0, 32);
        lv_obj_set_style_text_color(wf_zen_date_, lv_color_hex(0x64748b), 0);
        lv_obj_set_style_text_font(wf_zen_date_, &lv_font_montserrat_16, 0);
    }

    UpdateWatchfaceUI();
}

void WatchPageManager::UpdateWatchfaceUI() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    static const char* const WEEKS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t.tm_hour, t.tm_min);
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%02d/%02d %s", t.tm_mon + 1, t.tm_mday, WEEKS[t.tm_wday % 7]);

    if (wf_style_ == WF_STYLE_BIG_DIGITAL) {
        if (wf_time_label_) lv_label_set_text(wf_time_label_, time_str);
        char sec_str[8];
        snprintf(sec_str, sizeof(sec_str), ":%02d", t.tm_sec);
        if (wf_sec_label_) {
            lv_label_set_text(wf_sec_label_, sec_str);
            lv_obj_set_style_text_color(wf_sec_label_, lv_color_hex(THEME_COLORS[wf_theme_color_idx_]), 0);
        }
        if (wf_sec_arc_) {
            lv_arc_set_value(wf_sec_arc_, t.tm_sec);
            lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(THEME_COLORS[wf_theme_color_idx_]), LV_PART_INDICATOR);
        }
        if (wf_date_label_) lv_label_set_text(wf_date_label_, date_str);
    } else if (wf_style_ == WF_STYLE_CLASSIC_ANALOG) {
        // Center is (140, 140)
        constexpr float cx = 140.0f;
        constexpr float cy = 140.0f;
        constexpr float pi = 3.14159265f;

        // Hour angle (0-360)
        float h_angle = ((t.tm_hour % 12) + t.tm_min / 60.0f) * 30.0f - 90.0f;
        float h_rad = h_angle * (pi / 180.0f);
        hour_pts_[0] = {(int16_t)cx, (int16_t)cy};
        hour_pts_[1] = {(int16_t)(cx + 52.0f * cosf(h_rad)), (int16_t)(cy + 52.0f * sinf(h_rad))};
        if (wf_hour_hand_) lv_line_set_points(wf_hour_hand_, hour_pts_, 2);

        // Min angle
        float m_angle = (t.tm_min + t.tm_sec / 60.0f) * 6.0f - 90.0f;
        float m_rad = m_angle * (pi / 180.0f);
        min_pts_[0] = {(int16_t)cx, (int16_t)cy};
        min_pts_[1] = {(int16_t)(cx + 82.0f * cosf(m_rad)), (int16_t)(cy + 82.0f * sinf(m_rad))};
        if (wf_min_hand_) lv_line_set_points(wf_min_hand_, min_pts_, 2);

        // Sec angle
        float s_angle = t.tm_sec * 6.0f - 90.0f;
        float s_rad = s_angle * (pi / 180.0f);
        sec_pts_[0] = {(int16_t)(cx - 18.0f * cosf(s_rad)), (int16_t)(cy - 18.0f * sinf(s_rad))};
        sec_pts_[1] = {(int16_t)(cx + 105.0f * cosf(s_rad)), (int16_t)(cy + 105.0f * sinf(s_rad))};
        if (wf_sec_hand_) lv_line_set_points(wf_sec_hand_, sec_pts_, 2);

        if (wf_date_label_) lv_label_set_text(wf_date_label_, date_str);
    } else if (wf_style_ == WF_STYLE_SPORT_RING) {
        if (wf_ring_sec_) lv_arc_set_value(wf_ring_sec_, t.tm_sec);
        if (wf_ring_min_) lv_arc_set_value(wf_ring_min_, t.tm_min);
        if (wf_ring_hour_) lv_arc_set_value(wf_ring_hour_, t.tm_hour);
        if (wf_sport_time_label_) lv_label_set_text(wf_sport_time_label_, time_str);
    } else if (wf_style_ == WF_STYLE_MINIMAL_ZEN) {
        if (wf_zen_time_) lv_label_set_text(wf_zen_time_, time_str);
        if (wf_zen_date_) lv_label_set_text(wf_zen_date_, date_str);
    }

    if (wf_bat_label_ && bat_cb_) {
        int bat = bat_cb_();
        char bat_str[16];
        snprintf(bat_str, sizeof(bat_str), "⚡ %d%%", bat);
        lv_label_set_text(wf_bat_label_, bat_str);
    }
}

// -------------------------------------------------------------
// 2. STOPWATCH (专业分段秒表)
// -------------------------------------------------------------
void WatchPageManager::CreateStopwatchScreen() {
    stopwatch_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(stopwatch_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(stopwatch_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(stopwatch_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Gestures: swipe right -> Watchface, swipe left -> Lucky Wheel
    lv_obj_add_event_cb(stopwatch_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_LEFT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_LUCKY_WHEEL, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            }
        }
    }, LV_EVENT_ALL, this);

    // Progress Arc around watch display
    sw_arc_ = lv_arc_create(stopwatch_screen_);
    lv_obj_set_size(sw_arc_, 272, 272);
    lv_obj_align(sw_arc_, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(sw_arc_, 270);
    lv_arc_set_bg_angles(sw_arc_, 0, 360);
    lv_arc_set_range(sw_arc_, 0, 60000);
    lv_obj_remove_style(sw_arc_, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(sw_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(sw_arc_, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(sw_arc_, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x10b981), LV_PART_INDICATOR);

    // Header Title
    lv_obj_t* title = lv_label_create(stopwatch_screen_);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_text_color(title, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "秒表");

    // Main Elapsed Time Label (MM:SS)
    sw_time_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_time_label_, LV_ALIGN_CENTER, -18, -32);
    lv_obj_set_style_text_color(sw_time_label_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(sw_time_label_, &lv_font_montserrat_40, 0);
    lv_label_set_text(sw_time_label_, "00:00");

    // Millisecond label (.00)
    sw_ms_label_ = lv_label_create(stopwatch_screen_);
    lv_obj_align_to(sw_ms_label_, sw_time_label_, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -6);
    lv_obj_set_style_text_color(sw_ms_label_, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_text_font(sw_ms_label_, &lv_font_montserrat_20, 0);
    lv_label_set_text(sw_ms_label_, ".00");

    // Left Button: LAP / RESET
    sw_btn_left_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_left_, 88, 44);
    lv_obj_align(sw_btn_left_, LV_ALIGN_CENTER, -55, 30);
    lv_obj_set_style_radius(sw_btn_left_, 22, 0);
    lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x334155), 0);
    sw_btn_left_label_ = lv_label_create(sw_btn_left_);
    lv_obj_center(sw_btn_left_label_);
    lv_obj_set_style_text_font(sw_btn_left_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(sw_btn_left_label_, "计圈");

    lv_obj_add_event_cb(sw_btn_left_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (self->haptic_cb_) self->haptic_cb_(40);
        if (self->sw_state_ == SW_RUNNING) {
            self->sw_laps_.push_back(self->sw_elapsed_ms_);
            self->UpdateStopwatchUI();
        } else if (self->sw_state_ == SW_PAUSED) {
            self->sw_state_ = SW_STOPPED;
            self->sw_elapsed_ms_ = 0;
            self->sw_laps_.clear();
            self->UpdateStopwatchUI();
        }
    }, LV_EVENT_CLICKED, this);

    // Right Button: START / STOP
    sw_btn_right_ = lv_btn_create(stopwatch_screen_);
    lv_obj_set_size(sw_btn_right_, 88, 44);
    lv_obj_align(sw_btn_right_, LV_ALIGN_CENTER, 55, 30);
    lv_obj_set_style_radius(sw_btn_right_, 22, 0);
    lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x065f46), 0);
    sw_btn_right_label_ = lv_label_create(sw_btn_right_);
    lv_obj_center(sw_btn_right_label_);
    lv_obj_set_style_text_font(sw_btn_right_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(sw_btn_right_label_, "开始");

    lv_obj_add_event_cb(sw_btn_right_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (self->haptic_cb_) self->haptic_cb_(40);
        if (self->sw_state_ == SW_STOPPED || self->sw_state_ == SW_PAUSED) {
            self->sw_state_ = SW_RUNNING;
            self->sw_start_tick_ = esp_timer_get_time() / 1000 - self->sw_elapsed_ms_;
            self->UpdateStopwatchUI();
        } else if (self->sw_state_ == SW_RUNNING) {
            self->sw_state_ = SW_PAUSED;
            self->sw_elapsed_ms_ = esp_timer_get_time() / 1000 - self->sw_start_tick_;
            self->UpdateStopwatchUI();
        }
    }, LV_EVENT_CLICKED, this);

    // Lap List Label
    sw_laps_list_ = lv_label_create(stopwatch_screen_);
    lv_obj_align(sw_laps_list_, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_text_color(sw_laps_list_, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(sw_laps_list_, &lv_font_montserrat_14, 0);
    lv_label_set_text(sw_laps_list_, "分段: --");
}

void WatchPageManager::UpdateStopwatchUI() {
    uint32_t total_ms = sw_elapsed_ms_;
    if (sw_state_ == SW_RUNNING) {
        total_ms = (esp_timer_get_time() / 1000) - sw_start_tick_;
        sw_elapsed_ms_ = total_ms;
    }

    uint32_t total_sec = total_ms / 1000;
    uint32_t minutes = total_sec / 60;
    uint32_t seconds = total_sec % 60;
    uint32_t centis = (total_ms % 1000) / 10;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
    if (sw_time_label_) lv_label_set_text(sw_time_label_, buf);

    snprintf(buf, sizeof(buf), ".%02lu", (unsigned long)centis);
    if (sw_ms_label_) lv_label_set_text(sw_ms_label_, buf);

    if (sw_arc_) {
        lv_arc_set_value(sw_arc_, total_ms % 60000);
    }

    if (sw_state_ == SW_RUNNING) {
        lv_label_set_text(sw_btn_left_label_, "计圈");
        lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x334155), 0);

        lv_label_set_text(sw_btn_right_label_, "停止");
        lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x991b1b), 0);
        lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0xef4444), LV_PART_INDICATOR);
    } else if (sw_state_ == SW_PAUSED) {
        lv_label_set_text(sw_btn_left_label_, "复位");
        lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x475569), 0);

        lv_label_set_text(sw_btn_right_label_, "继续");
        lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x065f46), 0);
        lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0xf59e0b), LV_PART_INDICATOR);
    } else {
        lv_label_set_text(sw_btn_left_label_, "计圈");
        lv_obj_set_style_bg_color(sw_btn_left_, lv_color_hex(0x1e293b), 0);

        lv_label_set_text(sw_btn_right_label_, "开始");
        lv_obj_set_style_bg_color(sw_btn_right_, lv_color_hex(0x065f46), 0);
        lv_obj_set_style_arc_color(sw_arc_, lv_color_hex(0x10b981), LV_PART_INDICATOR);
    }

    if (sw_laps_list_) {
        if (!sw_laps_.empty()) {
            uint32_t last_lap = sw_laps_.back();
            uint32_t lm = (last_lap / 1000) / 60;
            uint32_t ls = (last_lap / 1000) % 60;
            uint32_t lms = (last_lap % 1000) / 10;
            char lap_buf[32];
            snprintf(lap_buf, sizeof(lap_buf), "圈 %d: %02lu:%02lu.%02lu", (int)sw_laps_.size(), (unsigned long)lm, (unsigned long)ls, (unsigned long)lms);
            lv_label_set_text(sw_laps_list_, lap_buf);
        } else {
            lv_label_set_text(sw_laps_list_, "分段: --");
        }
    }
}

// -------------------------------------------------------------
// 3. LUCKY WHEEL (幸运抽奖大转盘)
// -------------------------------------------------------------
void WatchPageManager::CreateLuckyWheelScreen() {
    lucky_wheel_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(lucky_wheel_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lucky_wheel_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(lucky_wheel_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Gesture: swipe right -> Stopwatch
    lv_obj_add_event_cb(lucky_wheel_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_STOPWATCH, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            }
        }
    }, LV_EVENT_ALL, this);

    // Header Title
    lv_obj_t* title = lv_label_create(lucky_wheel_screen_);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 22);
    lv_obj_set_style_text_color(title, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_label_set_text(title, "幸运大转盘");

    // Outer rotating arc
    wheel_arc_ = lv_arc_create(lucky_wheel_screen_);
    lv_obj_set_size(wheel_arc_, 230, 230);
    lv_obj_center(wheel_arc_);
    lv_arc_set_rotation(wheel_arc_, 0);
    lv_arc_set_bg_angles(wheel_arc_, 0, 360);
    lv_arc_set_range(wheel_arc_, 0, 360);
    lv_obj_remove_style(wheel_arc_, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(wheel_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(wheel_arc_, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_arc_width(wheel_arc_, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(wheel_arc_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);

    // Center Big Result / Prompt Label
    wheel_result_label_ = lv_label_create(lucky_wheel_screen_);
    lv_obj_align(wheel_result_label_, LV_ALIGN_CENTER, 0, -18);
    lv_obj_set_style_text_color(wheel_result_label_, lv_color_hex(0xfacc15), 0);
    lv_obj_set_style_text_font(wheel_result_label_, &font_noto_sans_basic_30_4, 0);
    lv_label_set_text(wheel_result_label_, "转一转");

    // Spin Button
    wheel_spin_btn_ = lv_btn_create(lucky_wheel_screen_);
    lv_obj_set_size(wheel_spin_btn_, 100, 42);
    lv_obj_align(wheel_spin_btn_, LV_ALIGN_CENTER, 0, 48);
    lv_obj_set_style_radius(wheel_spin_btn_, 21, 0);
    lv_obj_set_style_bg_color(wheel_spin_btn_, lv_color_hex(0x2563eb), 0);
    lv_obj_t* btn_lbl = lv_label_create(wheel_spin_btn_);
    lv_obj_center(btn_lbl);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(btn_lbl, "摇奖");

    lv_obj_add_event_cb(wheel_spin_btn_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self || self->wheel_spinning_) return;
        if (self->haptic_cb_) self->haptic_cb_(50);

        self->wheel_spinning_ = true;
        self->wheel_speed_ = 35.0f + (esp_random() % 25); // Initial angular velocity (deg/frame)
        self->wheel_chosen_idx_ = esp_random() % 6;
        lv_label_set_text(self->wheel_result_label_, "转动中...");
    }, LV_EVENT_CLICKED, this);
}

void WatchPageManager::UpdateLuckyWheelUI() {
    if (!wheel_spinning_) return;

    wheel_angle_ += wheel_speed_;
    while (wheel_angle_ >= 360.0f) wheel_angle_ -= 360.0f;

    if (wheel_arc_) {
        lv_arc_set_rotation(wheel_arc_, (int)wheel_angle_);
        lv_arc_set_value(wheel_arc_, 60); // Show segment length
    }

    // Deceleration model
    wheel_speed_ *= 0.965f;

    if (wheel_speed_ < 0.6f) {
        wheel_spinning_ = false;
        wheel_speed_ = 0.0f;
        if (haptic_cb_) haptic_cb_(80);
        if (wheel_result_label_ && wheel_chosen_idx_ >= 0 && wheel_chosen_idx_ < 6) {
            lv_label_set_text(wheel_result_label_, WHEEL_OPTIONS[wheel_chosen_idx_]);
        }
    }
}

// -------------------------------------------------------------
// Periodical Timers
// -------------------------------------------------------------
void WatchPageManager::Update1s() {
    if (current_page_ == WATCH_PAGE_WATCHFACE) {
        UpdateWatchfaceUI();
    }
}

void WatchPageManager::Update30ms() {
    if (current_page_ == WATCH_PAGE_STOPWATCH && sw_state_ == SW_RUNNING) {
        UpdateStopwatchUI();
    } else if (current_page_ == WATCH_PAGE_LUCKY_WHEEL && wheel_spinning_) {
        UpdateLuckyWheelUI();
    }
}

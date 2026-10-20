#include "watch_apps.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <time.h>
#include <sys/time.h>
#include <cstdio>

#define TAG "WatchApps"

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

    // Background timers for UI updates
    lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<WatchPageManager*>(lv_timer_get_user_data(t));
        if (self) self->Update1s();
    }, 1000, this);

    lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<WatchPageManager*>(lv_timer_get_user_data(t));
        if (self) self->Update50ms();
    }, 50, this);
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
        default:
            target = muse_screen_;
            break;
    }
    if (target != nullptr) {
        lv_screen_load_anim(target, anim, 220, 0, false);
    }
}

bool WatchPageManager::HandleBack() {
    if (current_page_ == WATCH_PAGE_STOPWATCH) {
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
// 1. WATCHFACE (表盘)
// -------------------------------------------------------------
static const uint32_t THEME_COLORS[] = {
    0x38bdf8, // 科技浅青 (Cyan)
    0xf59e0b, // 活力暖橙 (Orange)
    0xa855f7, // 极光幻紫 (Purple)
    0x22c55e, // 清新翡翠 (Green)
};

void WatchPageManager::CreateWatchfaceScreen() {
    watchface_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(watchface_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(watchface_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(watchface_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Gestures: swipe right -> Muse, swipe left -> Stopwatch, click center -> change theme
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
        } else if (code == LV_EVENT_CLICKED) {
            self->wf_theme_color_idx_ = (self->wf_theme_color_idx_ + 1) % 4;
            if (self->haptic_cb_) self->haptic_cb_(40);
            self->UpdateWatchfaceUI();
        }
    }, LV_EVENT_ALL, this);

    // Outer second arc
    wf_sec_arc_ = lv_arc_create(watchface_screen_);
    lv_obj_set_size(wf_sec_arc_, 272, 272);
    lv_obj_align(wf_sec_arc_, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(wf_sec_arc_, 270);
    lv_arc_set_bg_angles(wf_sec_arc_, 0, 360);
    lv_arc_set_range(wf_sec_arc_, 0, 60);
    lv_obj_remove_style(wf_sec_arc_, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(wf_sec_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(wf_sec_arc_, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(0x1e293b), LV_PART_MAIN);
    lv_obj_set_style_arc_width(wf_sec_arc_, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(THEME_COLORS[0]), LV_PART_INDICATOR);

    // Top battery info
    wf_bat_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_bat_label_, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_text_color(wf_bat_label_, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(wf_bat_label_, &lv_font_montserrat_14, 0);
    lv_label_set_text(wf_bat_label_, "100%");

    // Center Big Time (HH:MM)
    wf_time_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_time_label_, LV_ALIGN_CENTER, -12, -8);
    lv_obj_set_style_text_color(wf_time_label_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(wf_time_label_, &lv_font_montserrat_48, 0);
    lv_label_set_text(wf_time_label_, "00:00");

    // Second badge
    wf_sec_label_ = lv_label_create(watchface_screen_);
    lv_obj_align_to(wf_sec_label_, wf_time_label_, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -8);
    lv_obj_set_style_text_color(wf_sec_label_, lv_color_hex(THEME_COLORS[0]), 0);
    lv_obj_set_style_text_font(wf_sec_label_, &lv_font_montserrat_18, 0);
    lv_label_set_text(wf_sec_label_, ":00");

    // Date & Weekday label
    wf_date_label_ = lv_label_create(watchface_screen_);
    lv_obj_align(wf_date_label_, LV_ALIGN_CENTER, 0, 46);
    lv_obj_set_style_text_color(wf_date_label_, lv_color_hex(0xa0aec0), 0);
    lv_obj_set_style_text_font(wf_date_label_, &lv_font_montserrat_16, 0);
    lv_label_set_text(wf_date_label_, "10-20 TUE");

    // Hint text at bottom
    lv_obj_t* hint = lv_label_create(watchface_screen_);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_label_set_text(hint, "< 小智  |  秒表 >");
}

void WatchPageManager::UpdateWatchfaceUI() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", t.tm_hour, t.tm_min);
    if (wf_time_label_) lv_label_set_text(wf_time_label_, time_str);

    char sec_str[8];
    snprintf(sec_str, sizeof(sec_str), ":%02d", t.tm_sec);
    if (wf_sec_label_) lv_label_set_text(wf_sec_label_, sec_str);

    if (wf_sec_arc_) {
        lv_arc_set_value(wf_sec_arc_, t.tm_sec);
        lv_obj_set_style_arc_color(wf_sec_arc_, lv_color_hex(THEME_COLORS[wf_theme_color_idx_]), LV_PART_INDICATOR);
    }
    if (wf_sec_label_) {
        lv_obj_set_style_text_color(wf_sec_label_, lv_color_hex(THEME_COLORS[wf_theme_color_idx_]), 0);
    }

    static const char* const WEEKS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%02d/%02d %s", t.tm_mon + 1, t.tm_mday, WEEKS[t.tm_wday % 7]);
    if (wf_date_label_) lv_label_set_text(wf_date_label_, date_str);

    if (wf_bat_label_ && bat_cb_) {
        int bat = bat_cb_();
        char bat_str[16];
        snprintf(bat_str, sizeof(bat_str), "⚡ %d%%", bat);
        lv_label_set_text(wf_bat_label_, bat_str);
    }
}

// -------------------------------------------------------------
// 2. STOPWATCH (秒表)
// -------------------------------------------------------------
void WatchPageManager::CreateStopwatchScreen() {
    stopwatch_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(stopwatch_screen_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(stopwatch_screen_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(stopwatch_screen_, LV_OBJ_FLAG_SCROLLABLE);

    // Gesture: swipe right -> Watchface
    lv_obj_add_event_cb(stopwatch_screen_, [](lv_event_t* e) {
        auto* self = static_cast<WatchPageManager*>(lv_event_get_user_data(e));
        if (!self) return;
        if (lv_event_get_code(e) == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_RIGHT) {
                if (self->haptic_cb_) self->haptic_cb_(30);
                self->SwitchToPage(WATCH_PAGE_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            }
        }
    }, LV_EVENT_ALL, this);

    // Progress Arc around watch display
    sw_arc_ = lv_arc_create(stopwatch_screen_);
    lv_obj_set_size(sw_arc_, 272, 272);
    lv_obj_align(sw_arc_, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_rotation(sw_arc_, 270);
    lv_arc_set_bg_angles(sw_arc_, 0, 360);
    lv_arc_set_range(sw_arc_, 0, 60000); // 60s per circle
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
            // Lap action
            self->sw_laps_.push_back(self->sw_elapsed_ms_);
            self->UpdateStopwatchUI();
        } else if (self->sw_state_ == SW_PAUSED) {
            // Reset action
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
            // Start
            self->sw_state_ = SW_RUNNING;
            self->sw_start_tick_ = esp_timer_get_time() / 1000 - self->sw_elapsed_ms_;
            self->UpdateStopwatchUI();
        } else if (self->sw_state_ == SW_RUNNING) {
            // Stop / Pause
            self->sw_state_ = SW_PAUSED;
            self->sw_elapsed_ms_ = esp_timer_get_time() / 1000 - self->sw_start_tick_;
            self->UpdateStopwatchUI();
        }
    }, LV_EVENT_CLICKED, this);

    // Lap List Label (shows last recorded lap)
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

    // Button state updates
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
// Periodical Timers
// -------------------------------------------------------------
void WatchPageManager::Update1s() {
    if (current_page_ == WATCH_PAGE_WATCHFACE) {
        UpdateWatchfaceUI();
    }
}

void WatchPageManager::Update50ms() {
    if (current_page_ == WATCH_PAGE_STOPWATCH && sw_state_ == SW_RUNNING) {
        UpdateStopwatchUI();
    }
}

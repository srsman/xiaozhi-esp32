/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <assets/assets.h>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <memory>
#include <string>

namespace view {

/**
 * @brief Dialog to prompt user to enter WiFi AP photo upload mode
 */
class GuruguruUploadDialog {
public:
    void init(lv_obj_t* parent);
    bool isConfirmed() const { return _is_confirmed; }
    bool isCancelled() const { return _is_cancelled; }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title_label;
    std::unique_ptr<uitk::lvgl_cpp::Label> _desc_label;
    std::unique_ptr<uitk::lvgl_cpp::Button> _confirm_button;
    std::unique_ptr<uitk::lvgl_cpp::Button> _cancel_button;
    bool _is_confirmed = false;
    bool _is_cancelled = false;
};

/**
 * @brief 9-Direction Guruguru Interactive Avatar View
 */
class GuruguruView {
public:
    GuruguruView() = default;
    ~GuruguruView() = default;

    void init(lv_obj_t* parent);
    void update();

    // Touch inputs
    void onTouch(int x, int y, bool isPressed);

    // IMU tilt tracking
    void updateImuTilt(float ax, float ay);

    // Dialog state
    bool consumeUploadRequested();
    void showUploadDialog();

    // Direction query
    int getCurrentDirection() const { return _current_dir; }
    void toggleImuMode();

private:
    void calculateDirection(int touchX, int touchY);
    void updateBlink();

    // Root panel
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;

    // Header & Badges
    std::unique_ptr<uitk::lvgl_cpp::Label> _title_badge;
    std::unique_ptr<uitk::lvgl_cpp::Label> _dir_badge;
    std::unique_ptr<uitk::lvgl_cpp::Label> _hint_label;

    // Avatar card & Ears
    std::unique_ptr<uitk::lvgl_cpp::Container> _avatar_card;
    std::unique_ptr<uitk::lvgl_cpp::Container> _ear_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _ear_right;
    std::unique_ptr<uitk::lvgl_cpp::Container> _head;

    // Face elements
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_right;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pupil_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pupil_right;
    std::unique_ptr<uitk::lvgl_cpp::Container> _sparkle_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _sparkle_right;

    std::unique_ptr<uitk::lvgl_cpp::Container> _blush_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _blush_right;
    std::unique_ptr<uitk::lvgl_cpp::Label> _mouth;

    // Upload dialog
    std::unique_ptr<GuruguruUploadDialog> _upload_dialog;
    bool _upload_requested = false;

    // State
    int _current_dir = 4; // 4 = Front
    int _pupil_offset_x = 0;
    int _pupil_offset_y = 0;
    bool _is_touching = false;
    bool _is_blinking = false;
    bool _imu_mode_enabled = true;

    uint32_t _last_blink_time = 0;
    uint32_t _next_blink_interval = 3200;
    uint32_t _last_touch_time = 0;

    static void handleViewEvent(lv_event_t* e);
};

} // namespace view

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <assets/assets.h>
#include <apps/common/arc_top_clock/arc_top_clock.h>
#include <smooth_lvgl.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory>
#include <string>
#include <vector>

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

    // Pass touch inputs
    void onTouchPress(int x, int y);
    void onTouchMove(int x, int y);
    void onTouchRelease();

    // IMU tilt tracking
    void updateImuTilt(float ax, float ay);

    // Dialog state
    bool consumeUploadRequested();
    void showUploadDialog();

    // Direction query
    int getCurrentDirection() const { return _current_dir; }
    bool isImuMode() const { return _imu_mode_enabled; }
    void toggleImuMode();

private:
    void calculateDirection(int touchX, int touchY);
    void renderAvatar();
    void updateBlink();

    // Root panel
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<view::ArcTopClock> _clock;

    // Head and Face Elements
    std::unique_ptr<uitk::lvgl_cpp::Container> _avatar_card;
    std::unique_ptr<uitk::lvgl_cpp::Container> _ear_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _ear_right;
    std::unique_ptr<uitk::lvgl_cpp::Container> _head;
    std::unique_ptr<uitk::lvgl_cpp::Container> _face_container;

    // Eyes
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_left_socket;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye_right_socket;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pupil_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _pupil_right;
    std::unique_ptr<uitk::lvgl_cpp::Container> _highlight_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _highlight_right;
    std::unique_ptr<uitk::lvgl_cpp::Label> _blink_eye_left;
    std::unique_ptr<uitk::lvgl_cpp::Label> _blink_eye_right;

    // Cheeks & Mouth
    std::unique_ptr<uitk::lvgl_cpp::Container> _blush_left;
    std::unique_ptr<uitk::lvgl_cpp::Container> _blush_right;
    std::unique_ptr<uitk::lvgl_cpp::Label> _mouth;
    std::unique_ptr<uitk::lvgl_cpp::Label> _reaction_icon;

    // Bottom UI Labels
    std::unique_ptr<uitk::lvgl_cpp::Label> _dir_badge;
    std::unique_ptr<uitk::lvgl_cpp::Label> _hint_label;

    // Upload dialog
    std::unique_ptr<GuruguruUploadDialog> _upload_dialog;
    bool _upload_requested = false;

    // State
    int _current_dir = 4; // 4 = Center
    float _target_pupil_x = 0.0f;
    float _target_pupil_y = 0.0f;
    float _curr_pupil_x = 0.0f;
    float _curr_pupil_y = 0.0f;

    bool _is_touching = false;
    bool _is_blinking = false;
    uint32_t _last_blink_time = 0;
    uint32_t _blink_duration_ms = 160;
    uint32_t _next_blink_interval = 3000;

    bool _imu_mode_enabled = true;
    uint32_t _last_touch_time = 0;

    static void handleViewEvent(lv_event_t* e);
};

} // namespace view

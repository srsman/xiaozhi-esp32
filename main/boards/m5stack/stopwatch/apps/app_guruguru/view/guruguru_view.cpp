/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <assets/assets.h>
#include <esp_log.h>
#include <hal/hal.h>
#include <cmath>
#include <cstdlib>

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr const char* TAG = "GuruguruView";

constexpr int _panel_size           = 466;
constexpr int _dialog_width         = 404;
constexpr int _dialog_height        = 220;
constexpr int _dialog_button_width  = 148;
constexpr int _dialog_button_height = 63;

constexpr uint32_t _dialog_bg_color      = 0x1E1E28;
constexpr uint32_t _dialog_border_color  = 0xFF69B4;
constexpr uint32_t _dialog_confirm_color = 0xFF69B4;
constexpr uint32_t _dialog_cancel_color  = 0x484856;

const char* getDirectionName(int dir)
{
    switch (dir) {
        case 0: return "TOP-LEFT";
        case 1: return "TOP";
        case 2: return "TOP-RIGHT";
        case 3: return "LEFT";
        case 4: return "FRONT (CENTER)";
        case 5: return "RIGHT";
        case 6: return "BOTTOM-LEFT";
        case 7: return "BOTTOM";
        case 8: return "BOTTOM-RIGHT";
        default: return "CENTER";
    }
}

} // namespace

/* -------------------------------------------------------------------------- */
/*                            GuruguruUploadDialog                            */
/* -------------------------------------------------------------------------- */
void GuruguruUploadDialog::init(lv_obj_t* parent)
{
    _is_confirmed = false;
    _is_cancelled = false;

    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_dialog_width, _dialog_height);
    _panel->setBgColor(lv_color_hex(_dialog_bg_color));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->setBorderColor(lv_color_hex(_dialog_border_color));
    _panel->setBorderWidth(3);
    _panel->setRadius(54);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->moveForeground();

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("Custom Avatar AP");
    _title_label->setTextFont(&MontserratSemiBold26);
    _title_label->setTextColor(lv_color_hex(0xFFFFFF));
    _title_label->align(LV_ALIGN_TOP_LEFT, 40, 32);

    _desc_label = std::make_unique<Label>(_panel->get());
    _desc_label->setText("Start AP to upload 9-dir slices?");
    _desc_label->setTextFont(&lv_font_montserrat_14);
    _desc_label->setTextColor(lv_color_hex(0xB0B0C0));
    _desc_label->align(LV_ALIGN_TOP_LEFT, 40, 70);

    // Confirm button (Upload / AP)
    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -92, 48);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBorderWidth(0);
    _confirm_button->setShadowWidth(0);
    _confirm_button->setBgColor(lv_color_hex(_dialog_confirm_color));
    _confirm_button->label().setText("Upload");
    _confirm_button->label().setTextFont(&lv_font_montserrat_24);
    _confirm_button->label().setTextColor(lv_color_hex(0xFFFFFF));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->onClick().connect([this]() { _is_confirmed = true; });

    // Cancel button
    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 92, 48);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBorderWidth(0);
    _cancel_button->setShadowWidth(0);
    _cancel_button->setBgColor(lv_color_hex(_dialog_cancel_color));
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_24);
    _cancel_button->label().setTextColor(lv_color_hex(0xD0D0D0));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->onClick().connect([this]() { _is_cancelled = true; });
}

/* -------------------------------------------------------------------------- */
/*                                GuruguruView                                */
/* -------------------------------------------------------------------------- */
void GuruguruView::init(lv_obj_t* parent)
{
    _upload_requested = false;
    _is_touching      = false;
    _is_blinking      = false;
    _current_dir      = 4;
    _curr_pupil_x     = 0.0f;
    _curr_pupil_y     = 0.0f;
    _target_pupil_x   = 0.0f;
    _target_pupil_y   = 0.0f;
    _last_blink_time  = GetHAL().millis();
    _last_touch_time  = GetHAL().millis();

    // 1. Root container (Full 466x466 circular canvas)
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(0x0C0C14));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Register touch & gesture events
    _panel->addEventCb(handleViewEvent, LV_EVENT_PRESSED, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_PRESSING, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_RELEASED, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_CLICKED, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_LONG_PRESSED, this);

    // 2. Clock Header
    _clock = std::make_unique<view::ArcTopClock>(_panel->get());
    _clock->init();
    _clock->align(LV_ALIGN_TOP_MID, 0, 6);
    _clock->addFlag(LV_OBJ_FLAG_FLOATING);

    // 3. Avatar Card Base (Circular Glow Background)
    _avatar_card = std::make_unique<Container>(_panel->get());
    _avatar_card->align(LV_ALIGN_CENTER, 0, -12);
    _avatar_card->setSize(340, 340);
    _avatar_card->setRadius(LV_RADIUS_CIRCLE);
    _avatar_card->setBorderWidth(3);
    _avatar_card->setBorderColor(lv_color_hex(0x3B2D54));
    _avatar_card->setBgColor(lv_color_hex(0x191428));
    _avatar_card->setBgOpa(LV_OPA_COVER);
    _avatar_card->setPaddingAll(0);
    _avatar_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _avatar_card->addFlag(LV_OBJ_FLAG_EVENT_BUBBLE);

    // 4. Cat Ears
    // Left Ear
    _ear_left = std::make_unique<Container>(_avatar_card->get());
    _ear_left->setSize(76, 76);
    _ear_left->setRadius(18);
    _ear_left->setBgColor(lv_color_hex(0xFFA6C9));
    _ear_left->setBorderColor(lv_color_hex(0xFF69B4));
    _ear_left->setBorderWidth(3);
    _ear_left->align(LV_ALIGN_TOP_LEFT, 52, 28);
    _ear_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Right Ear
    _ear_right = std::make_unique<Container>(_avatar_card->get());
    _ear_right->setSize(76, 76);
    _ear_right->setRadius(18);
    _ear_right->setBgColor(lv_color_hex(0xFFA6C9));
    _ear_right->setBorderColor(lv_color_hex(0xFF69B4));
    _ear_right->setBorderWidth(3);
    _ear_right->align(LV_ALIGN_TOP_RIGHT, -52, 28);
    _ear_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 5. Head Container (Round Cute Anime Head)
    _head = std::make_unique<Container>(_avatar_card->get());
    _head->setSize(268, 268);
    _head->setRadius(LV_RADIUS_CIRCLE);
    _head->align(LV_ALIGN_CENTER, 0, 18);
    _head->setBgColor(lv_color_hex(0xFFF9FB));
    _head->setBorderColor(lv_color_hex(0xFFD1DC));
    _head->setBorderWidth(4);
    _head->setPaddingAll(0);
    _head->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 6. Floating Face Container (Parallax Movement)
    _face_container = std::make_unique<Container>(_head->get());
    _face_container->setSize(230, 200);
    _face_container->align(LV_ALIGN_CENTER, 0, 10);
    _face_container->setBorderWidth(0);
    _face_container->setBgOpa(0);
    _face_container->setPaddingAll(0);
    _face_container->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 7. Cheeks (Cute Pink Blushing)
    _blush_left = std::make_unique<Container>(_face_container->get());
    _blush_left->setSize(34, 18);
    _blush_left->setRadius(LV_RADIUS_CIRCLE);
    _blush_left->setBgColor(lv_color_hex(0xFF8DA1));
    _blush_left->setBgOpa(160);
    _blush_left->setBorderWidth(0);
    _blush_left->align(LV_ALIGN_CENTER, -66, 28);

    _blush_right = std::make_unique<Container>(_face_container->get());
    _blush_right->setSize(34, 18);
    _blush_right->setRadius(LV_RADIUS_CIRCLE);
    _blush_right->setBgColor(lv_color_hex(0xFF8DA1));
    _blush_right->setBgOpa(160);
    _blush_right->setBorderWidth(0);
    _blush_right->align(LV_ALIGN_CENTER, 66, 28);

    // 8. Eyes
    // Left Eye Socket
    _eye_left_socket = std::make_unique<Container>(_face_container->get());
    _eye_left_socket->setSize(52, 68);
    _eye_left_socket->setRadius(26);
    _eye_left_socket->align(LV_ALIGN_CENTER, -48, -4);
    _eye_left_socket->setBgColor(lv_color_hex(0x231834));
    _eye_left_socket->setBorderColor(lv_color_hex(0x110A1C));
    _eye_left_socket->setBorderWidth(2);
    _eye_left_socket->setPaddingAll(0);
    _eye_left_socket->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Left Pupil
    _pupil_left = std::make_unique<Container>(_eye_left_socket->get());
    _pupil_left->setSize(34, 44);
    _pupil_left->setRadius(18);
    _pupil_left->align(LV_ALIGN_CENTER, 0, 0);
    _pupil_left->setBgColor(lv_color_hex(0x8A2BE2)); // Violet gradient tone
    _pupil_left->setBorderWidth(0);

    // Left Sparkle Highlight
    _highlight_left = std::make_unique<Container>(_pupil_left->get());
    _highlight_left->setSize(14, 14);
    _highlight_left->setRadius(LV_RADIUS_CIRCLE);
    _highlight_left->align(LV_ALIGN_TOP_LEFT, 4, 4);
    _highlight_left->setBgColor(lv_color_hex(0xFFFFFF));
    _highlight_left->setBorderWidth(0);

    // Right Eye Socket
    _eye_right_socket = std::make_unique<Container>(_face_container->get());
    _eye_right_socket->setSize(52, 68);
    _eye_right_socket->setRadius(26);
    _eye_right_socket->align(LV_ALIGN_CENTER, 48, -4);
    _eye_right_socket->setBgColor(lv_color_hex(0x231834));
    _eye_right_socket->setBorderColor(lv_color_hex(0x110A1C));
    _eye_right_socket->setBorderWidth(2);
    _eye_right_socket->setPaddingAll(0);
    _eye_right_socket->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Right Pupil
    _pupil_right = std::make_unique<Container>(_eye_right_socket->get());
    _pupil_right->setSize(34, 44);
    _pupil_right->setRadius(18);
    _pupil_right->align(LV_ALIGN_CENTER, 0, 0);
    _pupil_right->setBgColor(lv_color_hex(0x8A2BE2));
    _pupil_right->setBorderWidth(0);

    // Right Sparkle Highlight
    _highlight_right = std::make_unique<Container>(_pupil_right->get());
    _highlight_right->setSize(14, 14);
    _highlight_right->setRadius(LV_RADIUS_CIRCLE);
    _highlight_right->align(LV_ALIGN_TOP_LEFT, 4, 4);
    _highlight_right->setBgColor(lv_color_hex(0xFFFFFF));
    _highlight_right->setBorderWidth(0);

    // Blink curve labels (^ ^) for blinking state
    _blink_eye_left = std::make_unique<Label>(_face_container->get());
    _blink_eye_left->setText("^");
    _blink_eye_left->setTextFont(&CommissionerMedium64);
    _blink_eye_left->setTextColor(lv_color_hex(0x231834));
    _blink_eye_left->align(LV_ALIGN_CENTER, -48, 12);
    _blink_eye_left->setHidden(true);

    _blink_eye_right = std::make_unique<Label>(_face_container->get());
    _blink_eye_right->setText("^");
    _blink_eye_right->setTextFont(&CommissionerMedium64);
    _blink_eye_right->setTextColor(lv_color_hex(0x231834));
    _blink_eye_right->align(LV_ALIGN_CENTER, 48, 12);
    _blink_eye_right->setHidden(true);

    // 9. Mouth (Cute Cat Smile)
    _mouth = std::make_unique<Label>(_face_container->get());
    _mouth->setText("w");
    _mouth->setTextFont(&lv_font_montserrat_24);
    _mouth->setTextColor(lv_color_hex(0x4A2E35));
    _mouth->align(LV_ALIGN_CENTER, 0, 36);

    // 10. Floating Heart Reaction Icon
    _reaction_icon = std::make_unique<Label>(_panel->get());
    _reaction_icon->setText("<3");
    _reaction_icon->setTextFont(&MontserratSemiBold26);
    _reaction_icon->setTextColor(lv_color_hex(0xFF1493));
    _reaction_icon->align(LV_ALIGN_CENTER, 110, -80);
    _reaction_icon->setHidden(true);

    // 11. Direction Badge at Bottom
    _dir_badge = std::make_unique<Label>(_panel->get());
    _dir_badge->setText("DIR: 4 [ FRONT ]");
    _dir_badge->setTextFont(&lv_font_montserrat_14);
    _dir_badge->setTextColor(lv_color_hex(0x00F0FF));
    _dir_badge->align(LV_ALIGN_BOTTOM_MID, 0, -38);

    // 12. Bottom Instruction Hint
    _hint_label = std::make_unique<Label>(_panel->get());
    _hint_label->setText("Touch / Drag to track | IMU: ON");
    _hint_label->setTextFont(&lv_font_montserrat_14);
    _hint_label->setTextColor(lv_color_hex(0x777788));
    _hint_label->align(LV_ALIGN_BOTTOM_MID, 0, -16);
}

void GuruguruView::calculateDirection(int touchX, int touchY)
{
    int dx = touchX - (_panel_size / 2);
    int dy = touchY - (_panel_size / 2);
    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

    if (dist < 42.0f) {
        // Deadzone -> Center
        _current_dir = 4;
        _target_pupil_x = 0.0f;
        _target_pupil_y = 0.0f;
        return;
    }

    // Limit pupil travel distance to eye socket radius
    float norm_x = static_cast<float>(dx) / dist;
    float norm_y = static_cast<float>(dy) / dist;
    float pupil_mag = std::min(dist / 140.0f, 1.0f);

    _target_pupil_x = norm_x * pupil_mag * 12.0f;
    _target_pupil_y = norm_y * pupil_mag * 16.0f;

    // Angle calculation: atan2 returns [-pi, +pi]
    float angle_rad = std::atan2(static_cast<float>(dy), static_cast<float>(dx));
    float angle_deg = angle_rad * 180.0f / 3.14159265f;
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }

    // 8 sectors (45 degrees each, centered on cardinal and diagonal axes)
    if (angle_deg >= 337.5f || angle_deg < 22.5f) {
        _current_dir = 5; // Right
    } else if (angle_deg >= 22.5f && angle_deg < 67.5f) {
        _current_dir = 8; // Bottom-Right
    } else if (angle_deg >= 67.5f && angle_deg < 112.5f) {
        _current_dir = 7; // Bottom
    } else if (angle_deg >= 112.5f && angle_deg < 157.5f) {
        _current_dir = 6; // Bottom-Left
    } else if (angle_deg >= 157.5f && angle_deg < 202.5f) {
        _current_dir = 3; // Left
    } else if (angle_deg >= 202.5f && angle_deg < 247.5f) {
        _current_dir = 0; // Top-Left
    } else if (angle_deg >= 247.5f && angle_deg < 292.5f) {
        _current_dir = 1; // Top
    } else {
        _current_dir = 2; // Top-Right
    }
}

void GuruguruView::onTouchPress(int x, int y)
{
    _is_touching = true;
    _last_touch_time = GetHAL().millis();
    calculateDirection(x, y);

    if (_reaction_icon) {
        _reaction_icon->setHidden(false);
    }
}

void GuruguruView::onTouchMove(int x, int y)
{
    _is_touching = true;
    _last_touch_time = GetHAL().millis();
    calculateDirection(x, y);
}

void GuruguruView::onTouchRelease()
{
    _is_touching = false;
    _last_touch_time = GetHAL().millis();
    _target_pupil_x = 0.0f;
    _target_pupil_y = 0.0f;
    _current_dir = 4; // Return to center

    if (_reaction_icon) {
        _reaction_icon->setHidden(true);
    }
}

void GuruguruView::updateImuTilt(float ax, float ay)
{
    if (!_imu_mode_enabled || _is_touching) {
        return;
    }

    // After user stops touching for > 600ms, allow IMU tilt to guide eye movement
    if (GetHAL().millis() - _last_touch_time < 600) {
        return;
    }

    float mag = std::sqrt(ax * ax + ay * ay);
    if (mag > 0.15f) {
        // Invert X because watch tilt left corresponds to gravity pulling right
        int sim_x = 233 + static_cast<int>(ax * 180.0f);
        int sim_y = 233 + static_cast<int>(ay * 180.0f);
        calculateDirection(sim_x, sim_y);
    } else {
        _current_dir = 4;
        _target_pupil_x = 0.0f;
        _target_pupil_y = 0.0f;
    }
}

void GuruguruView::toggleImuMode()
{
    _imu_mode_enabled = !_imu_mode_enabled;
    if (_hint_label) {
        _hint_label->setText(_imu_mode_enabled ? "Touch / Drag | IMU: ON" : "Touch / Drag | IMU: OFF");
    }
    GetHAL().vibrate(40);
}

void GuruguruView::updateBlink()
{
    uint32_t now = GetHAL().millis();
    if (!_is_blinking) {
        if (now - _last_blink_time > _next_blink_interval) {
            _is_blinking = true;
            _last_blink_time = now;
            _next_blink_interval = 2400 + (std::rand() % 2600); // 2.4s ~ 5.0s random interval

            // Show closed blink curves, hide open eye sockets
            if (_eye_left_socket) _eye_left_socket->setHidden(true);
            if (_eye_right_socket) _eye_right_socket->setHidden(true);
            if (_blink_eye_left) _blink_eye_left->setHidden(false);
            if (_blink_eye_right) _blink_eye_right->setHidden(false);
        }
    } else {
        if (now - _last_blink_time > _blink_duration_ms) {
            _is_blinking = false;
            _last_blink_time = now;

            // Restore open eyes
            if (_eye_left_socket) _eye_left_socket->setHidden(false);
            if (_eye_right_socket) _eye_right_socket->setHidden(false);
            if (_blink_eye_left) _blink_eye_left->setHidden(true);
            if (_blink_eye_right) _blink_eye_right->setHidden(true);
        }
    }
}

void GuruguruView::renderAvatar()
{
    // Low-pass filter for analog pupil motion
    _curr_pupil_x += (_target_pupil_x - _curr_pupil_x) * 0.38f;
    _curr_pupil_y += (_target_pupil_y - _curr_pupil_y) * 0.38f;

    int px = static_cast<int>(_curr_pupil_x);
    int py = static_cast<int>(_curr_pupil_y);

    if (_pupil_left) {
        _pupil_left->align(LV_ALIGN_CENTER, px, py);
    }
    if (_pupil_right) {
        _pupil_right->align(LV_ALIGN_CENTER, px, py);
    }

    // Parallax face offset
    if (_face_container) {
        _face_container->align(LV_ALIGN_CENTER, px / 4, 10 + py / 5);
    }

    // Update direction badge text
    if (_dir_badge) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DIR: %d [ %s ]", _current_dir, getDirectionName(_current_dir));
        _dir_badge->setText(buf);
    }
}

void GuruguruView::update()
{
    if (_upload_dialog) {
        if (_upload_dialog->isConfirmed()) {
            ESP_LOGI(TAG, "Upload dialog confirmed");
            _upload_requested = true;
            _upload_dialog.reset();
            return;
        }
        if (_upload_dialog->isCancelled()) {
            _upload_dialog.reset();
            return;
        }
    }

    updateBlink();
    renderAvatar();
}

bool GuruguruView::consumeUploadRequested()
{
    bool req = _upload_requested;
    _upload_requested = false;
    return req;
}

void GuruguruView::showUploadDialog()
{
    if (_upload_dialog != nullptr) return;
    _upload_dialog = std::make_unique<GuruguruUploadDialog>();
    _upload_dialog->init(_panel ? _panel->get() : lv_screen_active());
}

void GuruguruView::handleViewEvent(lv_event_t* e)
{
    auto* self = static_cast<GuruguruView*>(lv_event_get_user_data(e));
    if (!self) return;

    if (self->_upload_dialog != nullptr) return;

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED) {
        self->showUploadDialog();
        return;
    }

    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_indev_active();
        if (indev) {
            lv_point_t pt;
            lv_indev_get_point(indev, &pt);
            if (code == LV_EVENT_PRESSED) {
                self->onTouchPress(pt.x, pt.y);
            } else {
                self->onTouchMove(pt.x, pt.y);
            }
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        self->onTouchRelease();
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        // Toggle IMU mode if tapped near bottom
        lv_indev_t* indev = lv_indev_active();
        if (indev) {
            lv_point_t pt;
            lv_indev_get_point(indev, &pt);
            if (pt.y > 380) {
                self->toggleImuMode();
            }
        }
        return;
    }
}

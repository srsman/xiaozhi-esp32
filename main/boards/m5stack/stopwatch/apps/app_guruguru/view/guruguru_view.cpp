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
constexpr int _dialog_width         = 380;
constexpr int _dialog_height        = 200;
constexpr int _dialog_button_width  = 130;
constexpr int _dialog_button_height = 54;

const char* getDirString(int dir)
{
    switch (dir) {
        case 0: return "TOP-LEFT";
        case 1: return "TOP";
        case 2: return "TOP-RIGHT";
        case 3: return "LEFT";
        case 4: return "FRONT";
        case 5: return "RIGHT";
        case 6: return "BOTTOM-LEFT";
        case 7: return "BOTTOM";
        case 8: return "BOTTOM-RIGHT";
        default: return "FRONT";
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
    _panel->setBgColor(lv_color_hex(0x181824));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->setBorderColor(lv_color_hex(0xFF69B4));
    _panel->setBorderWidth(2);
    _panel->setRadius(40);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->moveForeground();

    _title_label = std::make_unique<Label>(_panel->get());
    _title_label->setText("Upload 9-Dir Slices");
    _title_label->setTextFont(&MontserratSemiBold26);
    _title_label->setTextColor(lv_color_hex(0xFFFFFF));
    _title_label->align(LV_ALIGN_TOP_MID, 0, 24);

    _desc_label = std::make_unique<Label>(_panel->get());
    _desc_label->setText("Start WiFi AP to upload custom images?");
    _desc_label->setTextFont(&lv_font_montserrat_14);
    _desc_label->setTextColor(lv_color_hex(0xA0A0B8));
    _desc_label->align(LV_ALIGN_TOP_MID, 0, 64);

    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -80, 42);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBgColor(lv_color_hex(0xFF69B4));
    _confirm_button->label().setText("Upload");
    _confirm_button->label().setTextFont(&lv_font_montserrat_20);
    _confirm_button->label().setTextColor(lv_color_hex(0xFFFFFF));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->onClick().connect([this]() { _is_confirmed = true; });

    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 80, 42);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBgColor(lv_color_hex(0x404050));
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_20);
    _cancel_button->label().setTextColor(lv_color_hex(0xD0D0D0));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->onClick().connect([this]() { _is_cancelled = true; });
}

/* -------------------------------------------------------------------------- */
/*                                GuruguruView                                */
/* -------------------------------------------------------------------------- */
void GuruguruView::init(lv_obj_t* parent)
{
    _upload_requested   = false;
    _is_touching        = false;
    _is_blinking        = false;
    _current_dir        = 4;
    _pupil_offset_x     = 0;
    _pupil_offset_y     = 0;
    _last_blink_time    = GetHAL().millis();
    _last_touch_time    = GetHAL().millis();

    // 1. Root container (Full 466x466 AMOLED display)
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_hex(0x0E0E18));
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Event callbacks
    _panel->addEventCb(handleViewEvent, LV_EVENT_PRESSED, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_PRESSING, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_RELEASED, this);
    _panel->addEventCb(handleViewEvent, LV_EVENT_LONG_PRESSED, this);

    // 2. Top Title Badge
    _title_badge = std::make_unique<Label>(_panel->get());
    _title_badge->setText("GURUGURU AVATAR");
    _title_badge->setTextFont(&MontserratSemiBold26);
    _title_badge->setTextColor(lv_color_hex(0xFF69B4));
    _title_badge->align(LV_ALIGN_TOP_MID, 0, 28);

    // 3. Avatar Card Base (Circular Glow Box)
    _avatar_card = std::make_unique<Container>(_panel->get());
    _avatar_card->align(LV_ALIGN_CENTER, 0, 6);
    _avatar_card->setSize(320, 320);
    _avatar_card->setRadius(LV_RADIUS_CIRCLE);
    _avatar_card->setBorderWidth(3);
    _avatar_card->setBorderColor(lv_color_hex(0x382A52));
    _avatar_card->setBgColor(lv_color_hex(0x1B172B));
    _avatar_card->setBgOpa(LV_OPA_COVER);
    _avatar_card->setPaddingAll(0);
    _avatar_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 4. Cat Ears
    _ear_left = std::make_unique<Container>(_avatar_card->get());
    _ear_left->setSize(68, 68);
    _ear_left->setRadius(16);
    _ear_left->setBgColor(lv_color_hex(0xFFA6C9));
    _ear_left->setBorderColor(lv_color_hex(0xFF69B4));
    _ear_left->setBorderWidth(3);
    _ear_left->align(LV_ALIGN_TOP_LEFT, 52, 28);
    _ear_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _ear_right = std::make_unique<Container>(_avatar_card->get());
    _ear_right->setSize(68, 68);
    _ear_right->setRadius(16);
    _ear_right->setBgColor(lv_color_hex(0xFFA6C9));
    _ear_right->setBorderColor(lv_color_hex(0xFF69B4));
    _ear_right->setBorderWidth(3);
    _ear_right->align(LV_ALIGN_TOP_RIGHT, -52, 28);
    _ear_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 5. Head (Round Cute Mascot Face)
    _head = std::make_unique<Container>(_avatar_card->get());
    _head->setSize(252, 252);
    _head->setRadius(LV_RADIUS_CIRCLE);
    _head->align(LV_ALIGN_CENTER, 0, 16);
    _head->setBgColor(lv_color_hex(0xFFF9FB));
    _head->setBorderColor(lv_color_hex(0xFFD2E0));
    _head->setBorderWidth(4);
    _head->setPaddingAll(0);
    _head->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 6. Blushing Cheeks
    _blush_left = std::make_unique<Container>(_head->get());
    _blush_left->setSize(32, 16);
    _blush_left->setRadius(LV_RADIUS_CIRCLE);
    _blush_left->setBgColor(lv_color_hex(0xFF8DA1));
    _blush_left->setBgOpa(160);
    _blush_left->setBorderWidth(0);
    _blush_left->align(LV_ALIGN_CENTER, -62, 30);
    _blush_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _blush_right = std::make_unique<Container>(_head->get());
    _blush_right->setSize(32, 16);
    _blush_right->setRadius(LV_RADIUS_CIRCLE);
    _blush_right->setBgColor(lv_color_hex(0xFF8DA1));
    _blush_right->setBgOpa(160);
    _blush_right->setBorderWidth(0);
    _blush_right->align(LV_ALIGN_CENTER, 62, 30);
    _blush_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 7. Eyes
    _eye_left = std::make_unique<Container>(_head->get());
    _eye_left->setSize(48, 64);
    _eye_left->setRadius(24);
    _eye_left->align(LV_ALIGN_CENTER, -46, -6);
    _eye_left->setBgColor(lv_color_hex(0x231834));
    _eye_left->setBorderColor(lv_color_hex(0x110A1C));
    _eye_left->setBorderWidth(2);
    _eye_left->setPaddingAll(0);
    _eye_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _pupil_left = std::make_unique<Container>(_eye_left->get());
    _pupil_left->setSize(30, 40);
    _pupil_left->setRadius(15);
    _pupil_left->align(LV_ALIGN_CENTER, 0, 0);
    _pupil_left->setBgColor(lv_color_hex(0x8A2BE2));
    _pupil_left->setBorderWidth(0);
    _pupil_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _sparkle_left = std::make_unique<Container>(_pupil_left->get());
    _sparkle_left->setSize(12, 12);
    _sparkle_left->setRadius(LV_RADIUS_CIRCLE);
    _sparkle_left->align(LV_ALIGN_TOP_LEFT, 4, 4);
    _sparkle_left->setBgColor(lv_color_hex(0xFFFFFF));
    _sparkle_left->setBorderWidth(0);
    _sparkle_left->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _eye_right = std::make_unique<Container>(_head->get());
    _eye_right->setSize(48, 64);
    _eye_right->setRadius(24);
    _eye_right->align(LV_ALIGN_CENTER, 46, -6);
    _eye_right->setBgColor(lv_color_hex(0x231834));
    _eye_right->setBorderColor(lv_color_hex(0x110A1C));
    _eye_right->setBorderWidth(2);
    _eye_right->setPaddingAll(0);
    _eye_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _pupil_right = std::make_unique<Container>(_eye_right->get());
    _pupil_right->setSize(30, 40);
    _pupil_right->setRadius(15);
    _pupil_right->align(LV_ALIGN_CENTER, 0, 0);
    _pupil_right->setBgColor(lv_color_hex(0x8A2BE2));
    _pupil_right->setBorderWidth(0);
    _pupil_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _sparkle_right = std::make_unique<Container>(_pupil_right->get());
    _sparkle_right->setSize(12, 12);
    _sparkle_right->setRadius(LV_RADIUS_CIRCLE);
    _sparkle_right->align(LV_ALIGN_TOP_LEFT, 4, 4);
    _sparkle_right->setBgColor(lv_color_hex(0xFFFFFF));
    _sparkle_right->setBorderWidth(0);
    _sparkle_right->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // 8. Mouth
    _mouth = std::make_unique<Label>(_head->get());
    _mouth->setText("w");
    _mouth->setTextFont(&lv_font_montserrat_24);
    _mouth->setTextColor(lv_color_hex(0x4A2E35));
    _mouth->align(LV_ALIGN_CENTER, 0, 36);

    // 9. Direction Indicator Badge
    _dir_badge = std::make_unique<Label>(_panel->get());
    _dir_badge->setText("DIR: 4 [ FRONT ]");
    _dir_badge->setTextFont(&lv_font_montserrat_14);
    _dir_badge->setTextColor(lv_color_hex(0x00F0FF));
    _dir_badge->align(LV_ALIGN_BOTTOM_MID, 0, -42);

    // 10. Hint Label
    _hint_label = std::make_unique<Label>(_panel->get());
    _hint_label->setText("Touch to track | Hold to upload | IMU: ON");
    _hint_label->setTextFont(&lv_font_montserrat_14);
    _hint_label->setTextColor(lv_color_hex(0x77778A));
    _hint_label->align(LV_ALIGN_BOTTOM_MID, 0, -20);
}

void GuruguruView::calculateDirection(int touchX, int touchY)
{
    int dx = touchX - (_panel_size / 2);
    int dy = touchY - (_panel_size / 2);
    float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

    if (dist < 38.0f) {
        _current_dir = 4; // Center
        _pupil_offset_x = 0;
        _pupil_offset_y = 0;
        return;
    }

    float norm_x = static_cast<float>(dx) / dist;
    float norm_y = static_cast<float>(dy) / dist;
    float mag = std::min(dist / 140.0f, 1.0f);

    _pupil_offset_x = static_cast<int>(norm_x * mag * 11.0f);
    _pupil_offset_y = static_cast<int>(norm_y * mag * 14.0f);

    float angle_deg = std::atan2(static_cast<float>(dy), static_cast<float>(dx)) * 180.0f / 3.14159265f;
    if (angle_deg < 0.0f) {
        angle_deg += 360.0f;
    }

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

void GuruguruView::onTouch(int x, int y, bool isPressed)
{
    _is_touching = isPressed;
    _last_touch_time = GetHAL().millis();

    if (isPressed) {
        calculateDirection(x, y);
    } else {
        _current_dir = 4;
        _pupil_offset_x = 0;
        _pupil_offset_y = 0;
    }

    if (_pupil_left)  _pupil_left->align(LV_ALIGN_CENTER, _pupil_offset_x, _pupil_offset_y);
    if (_pupil_right) _pupil_right->align(LV_ALIGN_CENTER, _pupil_offset_x, _pupil_offset_y);

    if (_dir_badge) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DIR: %d [ %s ]", _current_dir, getDirString(_current_dir));
        _dir_badge->setText(buf);
    }
}

void GuruguruView::updateImuTilt(float ax, float ay)
{
    if (!_imu_mode_enabled || _is_touching) {
        return;
    }

    if (GetHAL().millis() - _last_touch_time < 500) {
        return;
    }

    float mag = std::sqrt(ax * ax + ay * ay);
    if (mag > 0.18f) {
        int sim_x = 233 + static_cast<int>(ax * 160.0f);
        int sim_y = 233 + static_cast<int>(ay * 160.0f);
        calculateDirection(sim_x, sim_y);
    } else {
        _current_dir = 4;
        _pupil_offset_x = 0;
        _pupil_offset_y = 0;
    }

    if (_pupil_left)  _pupil_left->align(LV_ALIGN_CENTER, _pupil_offset_x, _pupil_offset_y);
    if (_pupil_right) _pupil_right->align(LV_ALIGN_CENTER, _pupil_offset_x, _pupil_offset_y);

    if (_dir_badge) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DIR: %d [ %s ]", _current_dir, getDirString(_current_dir));
        _dir_badge->setText(buf);
    }
}

void GuruguruView::toggleImuMode()
{
    _imu_mode_enabled = !_imu_mode_enabled;
    if (_hint_label) {
        _hint_label->setText(_imu_mode_enabled ? "Touch to track | Hold to upload | IMU: ON"
                                              : "Touch to track | Hold to upload | IMU: OFF");
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
            _next_blink_interval = 2600 + (std::rand() % 2400);

            // Morph eye socket into closed smiling slit
            if (_eye_left)  _eye_left->setHeight(8);
            if (_eye_right) _eye_right->setHeight(8);
            if (_pupil_left)  _pupil_left->setHidden(true);
            if (_pupil_right) _pupil_right->setHidden(true);
        }
    } else {
        if (now - _last_blink_time > 150) {
            _is_blinking = false;
            _last_blink_time = now;

            // Restore full open eyes
            if (_eye_left)  _eye_left->setHeight(64);
            if (_eye_right) _eye_right->setHeight(64);
            if (_pupil_left)  _pupil_left->setHidden(false);
            if (_pupil_right) _pupil_right->setHidden(false);
        }
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
            self->onTouch(pt.x, pt.y, true);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        self->onTouch(233, 233, false);
        return;
    }
}

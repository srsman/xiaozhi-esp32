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

using namespace view;
using namespace uitk::lvgl_cpp;

namespace {

constexpr int _panel_size           = 466;
constexpr int _hint_width           = 320;
constexpr int _dialog_width         = 404;
constexpr int _dialog_height        = 202;
constexpr int _dialog_button_width  = 148;
constexpr int _dialog_button_height = 63;

constexpr uint32_t _dialog_bg_color      = 0x2B2B2B;
constexpr uint32_t _dialog_border_color  = 0x6A6A6A;
constexpr uint32_t _dialog_confirm_color = 0x4AD78C;
constexpr uint32_t _dialog_cancel_color  = 0x515151;
constexpr uint32_t _label_color          = 0xFFFFFF;
constexpr uint32_t _confirm_text_color   = 0x0F5831;
constexpr uint32_t _hint_text_color      = 0xD9D9D9;

}  // namespace

void EditBadgeDialog::init(lv_obj_t* parent)
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
    _panel->setRadius(58);
    _panel->setPaddingAll(0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _panel->moveForeground();

    _label = std::make_unique<Label>(_panel->get());
    _label->setText("Enter badge edit?");
    _label->setTextFont(&MontserratSemiBold26);
    _label->setTextColor(lv_color_hex(_label_color));
    _label->align(LV_ALIGN_TOP_LEFT, 49, 40);

    _confirm_button = std::make_unique<Button>(_panel->get());
    _confirm_button->setSize(_dialog_button_width, _dialog_button_height);
    _confirm_button->align(LV_ALIGN_CENTER, -92, 36);
    _confirm_button->setRadius(LV_RADIUS_CIRCLE);
    _confirm_button->setBorderWidth(0);
    _confirm_button->setShadowWidth(0);
    _confirm_button->setBgColor(lv_color_hex(_dialog_confirm_color));
    _confirm_button->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _confirm_button->addFlag(LV_OBJ_FLAG_CLICKABLE);
    _confirm_button->label().setText("Edit");
    _confirm_button->label().setTextFont(&lv_font_montserrat_24);
    _confirm_button->label().setTextColor(lv_color_hex(_confirm_text_color));
    _confirm_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _confirm_button->label().removeFlag(LV_OBJ_FLAG_CLICKABLE);

    auto on_confirm = [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        ESP_LOGI("EditBadgeDialog", "Confirm clicked! code=%d", (int)code);
        auto* self = static_cast<EditBadgeDialog*>(lv_event_get_user_data(e));
        if (self) {
            self->_is_confirmed = true;
        }
    };
    _confirm_button->addEventCb(on_confirm, LV_EVENT_CLICKED, this);
    _confirm_button->addEventCb(on_confirm, LV_EVENT_SHORT_CLICKED, this);
    _confirm_button->addEventCb(on_confirm, LV_EVENT_RELEASED, this);
    _confirm_button->onClick().connect([this]() {
        ESP_LOGI("EditBadgeDialog", "Confirm onClick signal");
        _is_confirmed = true;
    });

    _cancel_button = std::make_unique<Button>(_panel->get());
    _cancel_button->setSize(_dialog_button_width, _dialog_button_height);
    _cancel_button->align(LV_ALIGN_CENTER, 92, 36);
    _cancel_button->setRadius(LV_RADIUS_CIRCLE);
    _cancel_button->setBorderWidth(0);
    _cancel_button->setShadowWidth(0);
    _cancel_button->setBgColor(lv_color_hex(_dialog_cancel_color));
    _cancel_button->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _cancel_button->addFlag(LV_OBJ_FLAG_CLICKABLE);
    _cancel_button->label().setText("Cancel");
    _cancel_button->label().setTextFont(&lv_font_montserrat_24);
    _cancel_button->label().setTextColor(lv_color_hex(_label_color));
    _cancel_button->label().align(LV_ALIGN_CENTER, 0, 0);
    _cancel_button->label().removeFlag(LV_OBJ_FLAG_CLICKABLE);

    auto on_cancel = [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        ESP_LOGI("EditBadgeDialog", "Cancel clicked! code=%d", (int)code);
        auto* self = static_cast<EditBadgeDialog*>(lv_event_get_user_data(e));
        if (self) {
            self->_is_cancelled = true;
        }
    };
    _cancel_button->addEventCb(on_cancel, LV_EVENT_CLICKED, this);
    _cancel_button->addEventCb(on_cancel, LV_EVENT_SHORT_CLICKED, this);
    _cancel_button->addEventCb(on_cancel, LV_EVENT_RELEASED, this);
    _cancel_button->onClick().connect([this]() {
        ESP_LOGI("EditBadgeDialog", "Cancel onClick signal");
        _is_cancelled = true;
    });
}

void BadgeView::init(lv_obj_t* parent)
{
    _edit_requested = false;
    _next_requested = false;
    _prev_requested = false;
    _is_pressing    = false;

    // Root screen container
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(_panel_size, _panel_size);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_black());
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Register gesture and touch events
    _panel->addEventCb(handleBadgeEvent, LV_EVENT_PRESSED, this);
    _panel->addEventCb(handleBadgeEvent, LV_EVENT_RELEASED, this);
    _panel->addEventCb(handleBadgeEvent, LV_EVENT_GESTURE, this);
    _panel->addEventCb(handleBadgeEvent, LV_EVENT_CLICKED, this);
    _panel->addEventCb(handleBadgeEvent, LV_EVENT_LONG_PRESSED, this);

    // Full screen image (for custom user-uploaded badges)
    _image = std::make_unique<Image>(_panel->get());
    _image->align(LV_ALIGN_CENTER, 0, 0);
    _image->setHidden(true);
    _image->addFlag(LV_OBJ_FLAG_CLICKABLE);
    _image->addFlag(LV_OBJ_FLAG_EVENT_BUBBLE);
    _image->addEventCb(handleBadgeEvent, LV_EVENT_PRESSED, this);
    _image->addEventCb(handleBadgeEvent, LV_EVENT_RELEASED, this);
    _image->addEventCb(handleBadgeEvent, LV_EVENT_GESTURE, this);
    _image->addEventCb(handleBadgeEvent, LV_EVENT_CLICKED, this);
    _image->addEventCb(handleBadgeEvent, LV_EVENT_LONG_PRESSED, this);

    // Native Geek Badge Card Container (380x380 round-corner card)
    _geek_card = std::make_unique<Container>(_panel->get());
    _geek_card->align(LV_ALIGN_CENTER, 0, 0);
    _geek_card->setSize(386, 386);
    _geek_card->setRadius(46);
    _geek_card->setBorderWidth(3);
    _geek_card->setBorderColor(lv_color_hex(0x00F0FF));
    _geek_card->setBgColor(lv_color_hex(0x0C1824));
    _geek_card->setBgOpa(LV_OPA_COVER);
    _geek_card->setPaddingAll(12);
    _geek_card->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _geek_card->addFlag(LV_OBJ_FLAG_EVENT_BUBBLE);
    _geek_card->addEventCb(handleBadgeEvent, LV_EVENT_PRESSED, this);
    _geek_card->addEventCb(handleBadgeEvent, LV_EVENT_RELEASED, this);
    _geek_card->addEventCb(handleBadgeEvent, LV_EVENT_GESTURE, this);
    _geek_card->addEventCb(handleBadgeEvent, LV_EVENT_CLICKED, this);
    _geek_card->addEventCb(handleBadgeEvent, LV_EVENT_LONG_PRESSED, this);

    // Tag at top of card
    _geek_chip_tag = std::make_unique<Label>(_geek_card->get());
    _geek_chip_tag->setText("[ XIAOZHI AI AGENT ]");
    _geek_chip_tag->setTextFont(&lv_font_montserrat_14);
    _geek_chip_tag->setTextColor(lv_color_hex(0x00F0FF));
    _geek_chip_tag->align(LV_ALIGN_TOP_MID, 0, 16);

    // Main title
    _geek_title = std::make_unique<Label>(_geek_card->get());
    _geek_title->setText("XIAOZHI 01");
    _geek_title->setTextFont(&MontserratSemiBold26);
    _geek_title->setTextColor(lv_color_hex(0xFFFFFF));
    _geek_title->align(LV_ALIGN_TOP_MID, 0, 38);

    // Central HD Icon
    _geek_icon = std::make_unique<Image>(_geek_card->get());
    _geek_icon->setSrc(&icon_xiaozhi);
    _geek_icon->align(LV_ALIGN_CENTER, 0, 5);

    // Status line
    _geek_status = std::make_unique<Label>(_geek_card->get());
    _geek_status->setText("● ONLINE  ·  ESP32-S3");
    _geek_status->setTextFont(&lv_font_montserrat_16);
    _geek_status->setTextColor(lv_color_hex(0x4AD78C));
    _geek_status->align(LV_ALIGN_BOTTOM_MID, 0, -42);

    // Sub-status / Footer
    _geek_footer = std::make_unique<Label>(_geek_card->get());
    _geek_footer->setText("CLEARANCE: LEVEL-9 // AUTH OK");
    _geek_footer->setTextFont(&lv_font_montserrat_14);
    _geek_footer->setTextColor(lv_color_hex(0x8B8B8B));
    _geek_footer->align(LV_ALIGN_BOTTOM_MID, 0, -18);

    // Left and Right subtle arrow indicators
    _left_arrow = std::make_unique<Label>(_panel->get());
    _left_arrow->setText("<");
    _left_arrow->setTextFont(&MontserratSemiBold26);
    _left_arrow->setTextColor(lv_color_hex(0x666666));
    _left_arrow->align(LV_ALIGN_LEFT_MID, 8, 0);

    _right_arrow = std::make_unique<Label>(_panel->get());
    _right_arrow->setText(">");
    _right_arrow->setTextFont(&MontserratSemiBold26);
    _right_arrow->setTextColor(lv_color_hex(0x666666));
    _right_arrow->align(LV_ALIGN_RIGHT_MID, -8, 0);

    // Dots indicator container
    _dots_container = std::make_unique<Container>(_panel->get());
    _dots_container->align(LV_ALIGN_BOTTOM_MID, 0, -12);
    _dots_container->setSize(200, 24);
    _dots_container->setBorderWidth(0);
    _dots_container->setBgOpa(0);
    _dots_container->setPaddingAll(0);
    _dots_container->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    // Hint label
    _edit_hint_label = std::make_unique<Label>(_panel->get());
    _edit_hint_label->setText("Hold to edit | Swipe to switch");
    _edit_hint_label->setTextFont(&lv_font_montserrat_14);
    _edit_hint_label->setTextColor(lv_color_hex(0x777777));
    _edit_hint_label->setWidth(_hint_width);
    _edit_hint_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _edit_hint_label->align(LV_ALIGN_TOP_MID, 0, 10);
}

void BadgeView::updatePageDots(size_t currentPos, size_t totalCount)
{
    if (totalCount == 0 || !_dots_container) {
        return;
    }

    if (_dots.size() != totalCount) {
        _dots.clear();
        const int dot_gap = 14;
        const int start_x = -static_cast<int>(totalCount - 1) * dot_gap / 2;

        for (size_t i = 0; i < totalCount; ++i) {
            auto dot = std::make_unique<Container>(_dots_container->get());
            dot->setSize(8, 8);
            dot->setRadius(LV_RADIUS_CIRCLE);
            dot->setBorderWidth(0);
            dot->setBgColor(lv_color_hex(0xFFFFFF));
            dot->align(LV_ALIGN_CENTER, start_x + static_cast<int>(i) * dot_gap, 0);
            _dots.push_back(std::move(dot));
        }
    }

    for (size_t i = 0; i < _dots.size(); ++i) {
        if (i == currentPos) {
            _dots[i]->setSize(16, 6);
            _dots[i]->setOpa(255);
            _dots[i]->setBgColor(lv_color_hex(0x00F0FF));
        } else {
            _dots[i]->setSize(6, 6);
            _dots[i]->setOpa(100);
            _dots[i]->setBgColor(lv_color_hex(0x888888));
        }
    }
}

void BadgeView::showGeekBadge(size_t index, size_t totalCount, size_t currentPos)
{
    if (_image) {
        _image->setHidden(true);
    }
    if (_geek_card) {
        _geek_card->setHidden(false);
    }

    index = index % 3;
    if (index == 0) {
        // Style 0: XiaoZhi Cyber Agent
        _geek_card->setBorderColor(lv_color_hex(0x00F0FF));
        _geek_card->setBgColor(lv_color_hex(0x0C1824));
        _geek_chip_tag->setText("[ XIAOZHI AI AGENT ]");
        _geek_chip_tag->setTextColor(lv_color_hex(0x00F0FF));
        _geek_title->setText("XIAOZHI 01");
        _geek_icon->setSrc(&icon_xiaozhi);
        _geek_status->setText("● ONLINE  ·  ESP32-S3");
        _geek_status->setTextColor(lv_color_hex(0x4AD78C));
        _geek_footer->setText("CLEARANCE: LEVEL-9 // AUTH OK");
    } else if (index == 1) {
        // Style 1: Cosmic Navigator Pass
        _geek_card->setBorderColor(lv_color_hex(0xFFD700));
        _geek_card->setBgColor(lv_color_hex(0x1F1133));
        _geek_chip_tag->setText("[ STAR NAVIGATOR ]");
        _geek_chip_tag->setTextColor(lv_color_hex(0xFFD700));
        _geek_title->setText("VOYAGER // PASS");
        _geek_icon->setSrc(&icon_badge);
        _geek_status->setText("● ORBIT: DEEP SPACE #07");
        _geek_status->setTextColor(lv_color_hex(0xE0A8FF));
        _geek_footer->setText("INTERSTELLAR PILOT CLEARANCE");
    } else {
        // Style 2: Matrix Hacker Badge
        _geek_card->setBorderColor(lv_color_hex(0x00FF66));
        _geek_card->setBgColor(lv_color_hex(0x071F11));
        _geek_chip_tag->setText("[ CYBER OPERATOR ]");
        _geek_chip_tag->setTextColor(lv_color_hex(0x00FF66));
        _geek_title->setText("MATRIX // ROOT");
        _geek_icon->setSrc(&icon_edit_badge);
        _geek_status->setText("● ACCESS: ROOT UNLOCKED");
        _geek_status->setTextColor(lv_color_hex(0x00FF66));
        _geek_footer->setText("DEBUG TERMINAL // READY");
    }

    updatePageDots(currentPos, totalCount);
}

void BadgeView::showUserImage(size_t slot, size_t totalCount, size_t currentPos)
{
    bool ok = GetHAL().loadBadgeSlot(imageObject(), slot);
    if (ok) {
        if (_geek_card) {
            _geek_card->setHidden(true);
        }
        if (_image) {
            _image->setHidden(false);
        }
    } else {
        ESP_LOGW("BadgeView", "Failed to load badge slot %u, gracefully falling back to geek badge", (unsigned)slot);
        showGeekBadge(0, totalCount, currentPos);
        if (_geek_status) {
            _geek_status->setText("● BADGE LOAD FAILED");
            _geek_status->setTextColor(lv_color_hex(0xFF5555));
        }
    }
    updatePageDots(currentPos, totalCount);
}

void BadgeView::setShowEditHint(bool show)
{
    if (_edit_hint_label) {
        _edit_hint_label->setHidden(!show);
    }
}

void BadgeView::update()
{
    if (!_edit_dialog) {
        return;
    }

    if (_edit_dialog->isConfirmed()) {
        ESP_LOGI("BadgeView", "Edit dialog confirmed -> edit_requested = true");
        _edit_requested = true;
        _edit_dialog.reset();
        return;
    }

    if (_edit_dialog->isCancelled()) {
        ESP_LOGI("BadgeView", "Edit dialog cancelled -> reset dialog");
        _edit_dialog.reset();
    }
}

bool BadgeView::consumeEditRequested()
{
    bool requested  = _edit_requested;
    _edit_requested = false;
    return requested;
}

bool BadgeView::consumeNextRequested()
{
    bool requested  = _next_requested;
    _next_requested = false;
    return requested;
}

bool BadgeView::consumePrevRequested()
{
    bool requested  = _prev_requested;
    _prev_requested = false;
    return requested;
}

void BadgeView::showEditDialog()
{
    if (_edit_dialog != nullptr) {
        return;
    }
    ESP_LOGI("BadgeView", "Opening edit dialog...");
    _edit_dialog = std::make_unique<EditBadgeDialog>();
    _edit_dialog->init(_panel ? _panel->get() : lv_screen_active());
}

void BadgeView::handleBadgeEvent(lv_event_t* e)
{
    auto* self = static_cast<BadgeView*>(lv_event_get_user_data(e));
    if (self == nullptr) {
        return;
    }

    if (self->_edit_dialog != nullptr) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED) {
        self->_is_pressing = false;
        self->showEditDialog();
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        if (dir == LV_DIR_LEFT) {
            self->_next_requested = true;
        } else if (dir == LV_DIR_RIGHT) {
            self->_prev_requested = true;
        }
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_t* indev = lv_indev_active();
        if (indev) {
            lv_indev_get_point(indev, &self->_press_point);
            self->_is_pressing = true;
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (self->_is_pressing) {
            self->_is_pressing = false;
            lv_point_t release_point = {0, 0};
            lv_indev_t* indev = lv_indev_active();
            if (indev) {
                lv_indev_get_point(indev, &release_point);
                int dx = release_point.x - self->_press_point.x;
                int dy = release_point.y - self->_press_point.y;
                if (std::abs(dx) > 40 && std::abs(dx) > std::abs(dy)) {
                    if (dx < 0) {
                        self->_next_requested = true;
                    } else {
                        self->_prev_requested = true;
                    }
                    return;
                }
            }
        }
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        // Also support tap left 25% or right 25% to switch
        lv_point_t click_point = {0, 0};
        lv_indev_t* indev = lv_indev_active();
        if (indev) {
            lv_indev_get_point(indev, &click_point);
            if (click_point.x < 110) {
                self->_prev_requested = true;
            } else if (click_point.x > 356) {
                self->_next_requested = true;
            }
        }
        return;
    }
}

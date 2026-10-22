/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <memory>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <vector>

namespace view {

class EditBadgeDialog {
public:
    void init(lv_obj_t* parent);
    bool isConfirmed() const
    {
        return _is_confirmed;
    }
    bool isCancelled() const
    {
        return _is_cancelled;
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label;
    std::unique_ptr<uitk::lvgl_cpp::Button> _confirm_button;
    std::unique_ptr<uitk::lvgl_cpp::Button> _cancel_button;
    bool _is_confirmed = false;
    bool _is_cancelled = false;
};

class BadgeView {
public:
    void init(lv_obj_t* parent = lv_screen_active());
    void setShowEditHint(bool show);
    void update();
    bool consumeEditRequested();
    bool consumeNextRequested();
    bool consumePrevRequested();

    lv_obj_t* imageObject() const
    {
        return _image ? _image->get() : nullptr;
    }

    void showGeekBadge(size_t index, size_t totalCount, size_t currentPos);
    void showUserImage(size_t slot, size_t totalCount, size_t currentPos);

private:
    void showEditDialog();
    static void handleBadgeEvent(lv_event_t* e);
    void updatePageDots(size_t currentPos, size_t totalCount);

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Image> _image;
    std::unique_ptr<uitk::lvgl_cpp::Label> _edit_hint_label;

    // Geek Badge Native UI
    std::unique_ptr<uitk::lvgl_cpp::Container> _geek_card;
    std::unique_ptr<uitk::lvgl_cpp::Label> _geek_chip_tag;
    std::unique_ptr<uitk::lvgl_cpp::Label> _geek_title;
    std::unique_ptr<uitk::lvgl_cpp::Image> _geek_icon;
    std::unique_ptr<uitk::lvgl_cpp::Label> _geek_status;
    std::unique_ptr<uitk::lvgl_cpp::Label> _geek_footer;

    // Navigation indicators
    std::unique_ptr<uitk::lvgl_cpp::Label> _left_arrow;
    std::unique_ptr<uitk::lvgl_cpp::Label> _right_arrow;
    std::unique_ptr<uitk::lvgl_cpp::Container> _dots_container;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _dots;

    std::unique_ptr<EditBadgeDialog> _edit_dialog;
    bool _edit_requested = false;
    bool _next_requested = false;
    bool _prev_requested = false;

    lv_point_t _press_point = {0, 0};
    bool _is_pressing = false;
};

}  // namespace view

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_badge.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <apps/common/loading_page/loading_page.h>
#include <memory>

using namespace mooncake;

AppBadge::AppBadge()
{
    setAppInfo().name = "Badge";
    setAppInfo().icon = (void*)&icon_badge;
}

void AppBadge::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppBadge::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    _badge_view = std::make_unique<view::BadgeView>();
    _badge_view->init(lv_screen_active());

    // Show initial geek badge or user image
    auto info = GetHAL().getBadgeCurrentInfo();
    if (info.isBuiltin) {
        _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
    } else {
        _badge_view->showUserImage(info.userImagePath.c_str(), info.totalCount, info.currentPos);
    }
}

void AppBadge::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    LvglLockGuard lock;

    if (_badge_view) {
        _badge_view->update();

        // Switch to next badge (via swipe left gesture, screen right tap, or physical key)
        if (event == input::KeyEvent::GoNext || _badge_view->consumeNextRequested()) {
            auto info = GetHAL().getBadgeNextInfo();
            if (info.isBuiltin) {
                _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
            } else {
                _badge_view->showUserImage(info.userImagePath.c_str(), info.totalCount, info.currentPos);
            }
        }
        // Switch to previous badge (via swipe right gesture, screen left tap, or physical key)
        else if (event == input::KeyEvent::GoPrevious || _badge_view->consumePrevRequested()) {
            auto info = GetHAL().getBadgePreviousInfo();
            if (info.isBuiltin) {
                _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
            } else {
                _badge_view->showUserImage(info.userImagePath.c_str(), info.totalCount, info.currentPos);
            }
        }

        // Long pressed -> Edit requested
        if (_badge_view->consumeEditRequested()) {
            mclog::info("start badge edit mode");
            auto loading_page = std::make_unique<view::LoadingPage>(0x151B22, 0xFFFFFF);
            loading_page->setMessage("Starting WiFi AP...\nPlease wait a moment");

            GetHAL().lvglUnlock();
            vTaskDelay(pdMS_TO_TICKS(50));

            GetHAL().startBadgeEditModeViaAp([&](std::string_view msg) {
                LvglLockGuard lock;
                loading_page->setMessage(msg);
            });

            GetHAL().lvglLock();
            loading_page.reset();

            // Refresh current badge image after exiting AP mode
            auto info = GetHAL().getBadgeCurrentInfo();
            if (info.isBuiltin) {
                _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
            } else {
                _badge_view->showUserImage(info.userImagePath.c_str(), info.totalCount, info.currentPos);
            }
        }
    }
}

void AppBadge::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;

    _badge_view.reset();
}

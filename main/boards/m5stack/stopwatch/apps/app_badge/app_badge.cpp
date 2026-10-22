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
#include <esp_log.h>
#include <memory>

static const char* TAG = "AppBadge";

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
    _is_ap_running = false;
    _ap_finished = false;
    _ap_task_handle = nullptr;

    LvglLockGuard lock;

    _badge_view = std::make_unique<view::BadgeView>();
    _badge_view->init(lv_screen_active());

    // Show initial geek badge or user image
    auto info = GetHAL().getBadgeCurrentInfo();
    if (info.isBuiltin) {
        _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
    } else {
        _badge_view->showUserImage(info.userSlot, info.totalCount, info.currentPos);
    }
}

void AppBadge::onRunning()
{
    input::KeyEvent event = input::KeyEvent::None;
    if (_key_manager) {
        event = _key_manager->update();
    }

    // 1. 如果当前处于 WiFi AP 配网图片编辑模式
    if (_is_ap_running) {
        // 用户按下实体按键退出 AP 模式
        if (event == input::KeyEvent::GoHome) {
            ESP_LOGI(TAG, "Home key pressed, requesting AP edit exit");
            GetHAL().requestBadgeEditExit();
        }

        // 检查后台 AP 任务是否已退出
        if (_ap_finished) {
            ESP_LOGI(TAG, "AP edit mode finished, restoring badge view");
            _is_ap_running = false;
            _ap_finished = false;

            LvglLockGuard lock;
            _loading_page.reset();

            // 恢复展示胸牌
            if (_badge_view) {
                auto info = GetHAL().getBadgeCurrentInfo();
                if (info.isBuiltin) {
                    _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
                } else {
                    _badge_view->showUserImage(info.userSlot, info.totalCount, info.currentPos);
                }
            }
        }
        return;
    }

    // 2. 正常胸牌浏览模式
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
                _badge_view->showUserImage(info.userSlot, info.totalCount, info.currentPos);
            }
        }
        // Switch to previous badge (via swipe right gesture, screen left tap, or physical key)
        else if (event == input::KeyEvent::GoPrevious || _badge_view->consumePrevRequested()) {
            auto info = GetHAL().getBadgePreviousInfo();
            if (info.isBuiltin) {
                _badge_view->showGeekBadge(info.builtinIndex, info.totalCount, info.currentPos);
            } else {
                _badge_view->showUserImage(info.userSlot, info.totalCount, info.currentPos);
            }
        }

        // 长按触发编辑对话框确认 -> 异步启动 AP 模式
        if (_badge_view->consumeEditRequested()) {
            ESP_LOGI(TAG, "consumeEditRequested triggered! Launching background AP task...");
            _is_ap_running = true;
            _ap_finished = false;

            _loading_page = std::make_unique<view::LoadingPage>(0x151B22, 0xFFFFFF);
            _loading_page->setMessage("Starting WiFi AP...\nPlease wait a moment");

            xTaskCreate([](void* arg) {
                auto* self = static_cast<AppBadge*>(arg);
                GetHAL().startBadgeEditModeViaAp([self](std::string_view msg) {
                    if (GetHAL().lvglLock()) {
                        if (self->_loading_page) {
                            self->_loading_page->setMessage(msg);
                        }
                        GetHAL().lvglUnlock();
                    }
                });
                ESP_LOGI(TAG, "AP task returned from startBadgeEditModeViaAp");
                vTaskDelay(pdMS_TO_TICKS(300));
                self->_ap_finished = true;
                self->_ap_task_handle = nullptr;
                vTaskDelete(nullptr);
            }, "badge_ap_task", 8192, this, 3, &_ap_task_handle);
        }
    }
}

void AppBadge::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_is_ap_running) {
        GetHAL().requestBadgeEditExit();
        int wait_count = 0;
        while (_ap_task_handle != nullptr && wait_count++ < 20) {
            GetHAL().delay(50);
        }
    }

    _key_manager.reset();

    LvglLockGuard lock;

    _loading_page.reset();
    _badge_view.reset();
}

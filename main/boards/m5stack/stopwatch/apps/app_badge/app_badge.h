/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <apps/common/loading_page/loading_page.h>
#include <mooncake.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <memory>

/**
 * @brief Derived App
 *
 */
class AppBadge : public mooncake::AppAbility {
public:
    AppBadge();

    // Override lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::BadgeView> _badge_view;
    std::unique_ptr<view::LoadingPage> _loading_page;
    std::atomic<bool> _is_ap_running{false};
    std::atomic<bool> _ap_finished{false};
    TaskHandle_t _ap_task_handle = nullptr;
};
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include "view/view.h"
#include <apps/common/key_manager/key_manager.h>
#include <apps/common/loading_page/loading_page.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake.h>
#include <memory>

/**
 * @brief Guruguru Interactive Avatar Mooncake App
 */
class AppGuruguru : public mooncake::AppAbility {
public:
    AppGuruguru();

    // Lifecycle callbacks
    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _key_manager;
    std::unique_ptr<view::GuruguruView> _view;
    std::unique_ptr<view::LoadingPage> _loading_page;

    bool _is_ap_running = false;
    bool _ap_finished = false;
    TaskHandle_t _ap_task_handle = nullptr;
};

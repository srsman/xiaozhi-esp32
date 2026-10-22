/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_guruguru.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <smooth_lvgl.hpp>

using namespace mooncake;

AppGuruguru::AppGuruguru()
{
    setAppInfo().name = "Guruguru";
    setAppInfo().icon = (void*)&icon_guruguru;
}

void AppGuruguru::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppGuruguru::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();
    _is_ap_running = false;
    _ap_finished = false;
    _ap_task_handle = nullptr;

    LvglLockGuard lock;
    _view = std::make_unique<view::GuruguruView>();
    _view->init(lv_screen_active());
}

void AppGuruguru::onRunning()
{
    input::KeyEvent key_event = input::KeyEvent::None;
    if (_key_manager) {
        key_event = _key_manager->update();
    }

    // 1. If currently in WiFi AP image upload mode
    if (_is_ap_running) {
        // Physical crown/home button exits AP mode
        if (key_event == input::KeyEvent::GoHome) {
            GetHAL().requestGuruguruUploadExit();
        }

        // Check if AP task finished
        if (_ap_finished) {
            _is_ap_running = false;
            _ap_finished = false;

            LvglLockGuard lock;
            _loading_page.reset();

            // Re-init view to show newly uploaded custom slices
            _view = std::make_unique<view::GuruguruView>();
            _view->init(lv_screen_active());
        }
        return;
    }

    // 2. Normal Avatar Mode: Hardware Button 1 (GoHome) exits back to Launcher
    if (key_event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    // Read IMU tilt outside LVGL lock
    GetHAL().updateImuData();
    const auto& imu = GetHAL().getImuData();

    LvglLockGuard lock;
    if (_view) {
        _view->updateImuTilt(imu.accelX, imu.accelY);
        _view->update();

        // Check if user confirmed entering upload mode from Setup dialog
        if (_view->consumeUploadRequested()) {
            _is_ap_running = true;
            _ap_finished = false;

            _view.reset(); // Destroy avatar view while uploading
            _loading_page = std::make_unique<view::LoadingPage>(0x0E0E18, 0xFF69B4);
            _loading_page->setMessage("Starting WiFi AP...\nPlease wait a moment");

            xTaskCreate([](void* arg) {
                auto* self = static_cast<AppGuruguru*>(arg);
                GetHAL().startGuruguruUploadModeViaAp([self](std::string_view msg) {
                    if (GetHAL().lvglLock()) {
                        if (self->_loading_page) {
                            self->_loading_page->setMessage(msg);
                        }
                        GetHAL().lvglUnlock();
                    }
                });
                vTaskDelay(pdMS_TO_TICKS(200));
                self->_ap_finished = true;
                self->_ap_task_handle = nullptr;
                vTaskDelete(nullptr);
            }, "guru_ap_task", 8192, this, 3, &_ap_task_handle);
        }
    }
}

void AppGuruguru::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_is_ap_running) {
        GetHAL().requestGuruguruUploadExit();
        int wait_count = 0;
        while (_ap_task_handle != nullptr && wait_count++ < 20) {
            GetHAL().delay(50);
        }
    }

    _key_manager.reset();

    LvglLockGuard lock;
    _loading_page.reset();
    _view.reset();
}

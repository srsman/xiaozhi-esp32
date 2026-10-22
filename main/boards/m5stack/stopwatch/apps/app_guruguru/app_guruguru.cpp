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

    // Hardware Button 1 (GoHome) exits back to Mooncake Launcher
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
    }
}

void AppGuruguru::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _key_manager.reset();

    LvglLockGuard lock;
    _view.reset();
}

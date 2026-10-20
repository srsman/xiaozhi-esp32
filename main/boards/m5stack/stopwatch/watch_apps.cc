/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#include "watch_apps.h"
#include "hal/hal.h"
#include <apps/apps.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <smooth_ui_toolkit.hpp>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "WatchApps";

using namespace mooncake;
using namespace smooth_ui_toolkit;

WatchPageManager& WatchPageManager::GetInstance() {
    static WatchPageManager instance;
    return instance;
}

void WatchPageManager::Init(lv_obj_t* muse_screen,
                            HapticCallback haptic_cb,
                            BatteryCallback bat_cb,
                            BrightnessCallback br_cb,
                            VolumeCallback vol_cb) {
    muse_screen_ = muse_screen;
    haptic_cb_ = haptic_cb;
    bat_cb_ = bat_cb;
    br_cb_ = br_cb;
    vol_cb_ = vol_cb;

    // 1. 设置 logger
    mclog::set_level(mclog::level_info);

    // 2. 初始化 HAL
    GetHAL().init();

    // 3. 设置 UI HAL 回调
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t ms) { GetHAL().delay(ms); });
    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return GetHAL().millis(); });

    // 4. 注册官方原生全套 10 大应用
    ESP_LOGI(TAG, "Installing official M5StopWatch Mooncake apps...");
    GetMooncake().installApp(std::make_unique<AppLauncher>());
    GetMooncake().installApp(std::make_unique<AppWatchFace>());
    GetMooncake().installApp(std::make_unique<AppStopWatch>());
    GetMooncake().installApp(std::make_unique<AppAlarmClock>());
    GetMooncake().installApp(std::make_unique<AppLuckyWheel>());
    GetMooncake().installApp(std::make_unique<AppFft>());
    GetMooncake().installApp(std::make_unique<AppBadge>());
    GetMooncake().installApp(std::make_unique<AppSetup>());
    GetMooncake().installApp(std::make_unique<AppImu>());
    GetMooncake().installApp(std::make_unique<AppFluidBox>());

    ESP_LOGI(TAG, "All official apps installed successfully!");

    // 5. 启动 Mooncake 主更新循环守护任务
    xTaskCreate([](void* arg) {
        while (true) {
            GetHAL().feedTheDog();
            if (GetHAL().lvglLock()) {
                mooncake::GetMooncake().update();
                GetHAL().lvglUnlock();
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }, "mooncake_task", 8192, nullptr, 2, nullptr);
}

void WatchPageManager::SetImuAccel(float ax, float ay, float az) {
    // 由 HAL Bridge 回调实时接管
}

bool WatchPageManager::HandleBack() {
    return false;
}

void WatchPageManager::SwitchToMuse() {
    is_in_muse_ = true;
    if (muse_screen_) {
        lv_screen_load_anim(muse_screen_, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    }
}

void WatchPageManager::SwitchToWatchFace() {
    is_in_muse_ = false;
}

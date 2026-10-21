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
                            VolumeCallback vol_cb,
                            UIVisibleCallback ui_cb,
                            ActionCallback start_chat_cb,
                            ActionCallback stop_chat_cb) {
    muse_screen_ = muse_screen;
    haptic_cb_ = haptic_cb;
    bat_cb_ = bat_cb;
    br_cb_ = br_cb;
    vol_cb_ = vol_cb;
    ui_cb_ = ui_cb;
    start_chat_cb_ = start_chat_cb;
    stop_chat_cb_ = stop_chat_cb;

    // 1. 设置 logger
    mclog::set_level(mclog::level_info);

    // 2. 初始化 HAL
    GetHAL().init();

    // 3. 设置 UI HAL 回调
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t ms) { GetHAL().delay(ms); });
    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return GetHAL().millis(); });

    // 4. 传递小智应用事件回调
    AppXiaoZhi::SetCallbacks(ui_cb_, start_chat_cb_, stop_chat_cb_);

    // 5. 注册 Mooncake 全套子应用（包含作为独立应用的 XiaoZhi）
    ESP_LOGI(TAG, "Installing M5StopWatch Mooncake apps (including XiaoZhi)...");
    watchface_app_id_ = GetMooncake().installApp(std::make_unique<AppWatchFace>());
    xiaozhi_app_id_ = GetMooncake().installApp(std::make_unique<AppXiaoZhi>());
    GetMooncake().installApp(std::make_unique<AppStopWatch>());
    GetMooncake().installApp(std::make_unique<AppAlarmClock>());
    GetMooncake().installApp(std::make_unique<AppLuckyWheel>());
    GetMooncake().installApp(std::make_unique<AppFft>());
    GetMooncake().installApp(std::make_unique<AppBadge>());
    GetMooncake().installApp(std::make_unique<AppSetup>());
    GetMooncake().installApp(std::make_unique<AppImu>());
    GetMooncake().installApp(std::make_unique<AppFluidBox>());

    // 关键：最后安装并启动 Launcher 桌面，以便其 getAppProps() 能够完整获取全部已安装的 10 个应用！
    launcher_app_id_ = GetMooncake().installApp(std::make_unique<AppLauncher>());

    ESP_LOGI(TAG, "All apps installed! Launcher ID: %d, WatchFace ID: %d, XiaoZhi ID: %d",
             launcher_app_id_, watchface_app_id_, xiaozhi_app_id_);

    // 6. 开机默认：隐藏小智界面，打开 Launcher 应用桌面（展示所有 10 大应用）
    if (ui_cb_) {
        ui_cb_(false);
    }
    if (launcher_app_id_ >= 0) {
        GetMooncake().openApp(launcher_app_id_);
    }

    // 7. 启动 Mooncake 主更新循环守护任务
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

bool WatchPageManager::IsInXiaoZhi() const {
    if (xiaozhi_app_id_ < 0) {
        return false;
    }
    return GetMooncake().getAppCurrentState(xiaozhi_app_id_) == AppAbility::StateRunning;
}

void WatchPageManager::SwitchToXiaoZhi() {
    if (xiaozhi_app_id_ < 0) {
        return;
    }
    if (IsInXiaoZhi()) {
        return;
    }

    if (GetHAL().lvglLock()) {
        // 先关闭除目标外的其他前台应用
        for (const auto& prop : GetMooncake().getAllAppProps()) {
            if (prop.appID != xiaozhi_app_id_ &&
                GetMooncake().getAppCurrentState(prop.appID) == AppAbility::StateRunning) {
                GetMooncake().closeApp(prop.appID);
            }
        }
        GetMooncake().openApp(xiaozhi_app_id_);
        GetHAL().lvglUnlock();
    }
    ESP_LOGI(TAG, "Switched to XiaoZhi app");
}

void WatchPageManager::SwitchToLauncher() {
    if (launcher_app_id_ < 0) {
        return;
    }

    if (GetHAL().lvglLock()) {
        for (const auto& prop : GetMooncake().getAllAppProps()) {
            if (prop.appID != launcher_app_id_ &&
                GetMooncake().getAppCurrentState(prop.appID) == AppAbility::StateRunning) {
                GetMooncake().closeApp(prop.appID);
            }
        }
        GetMooncake().openApp(launcher_app_id_);
        GetHAL().lvglUnlock();
    }
    ESP_LOGI(TAG, "Switched to Launcher");
}

void WatchPageManager::SwitchToWatchFace() {
    if (watchface_app_id_ < 0) {
        SwitchToLauncher();
        return;
    }

    if (GetHAL().lvglLock()) {
        for (const auto& prop : GetMooncake().getAllAppProps()) {
            if (prop.appID != watchface_app_id_ &&
                GetMooncake().getAppCurrentState(prop.appID) == AppAbility::StateRunning) {
                GetMooncake().closeApp(prop.appID);
            }
        }
        GetMooncake().openApp(watchface_app_id_);
        GetHAL().lvglUnlock();
    }
    ESP_LOGI(TAG, "Switched to WatchFace");
}

void WatchPageManager::TogglePage() {
    if (IsInXiaoZhi()) {
        SwitchToLauncher();
    } else {
        SwitchToXiaoZhi();
    }
}

bool WatchPageManager::HandleBack() {
    SwitchToLauncher();
    return true;
}


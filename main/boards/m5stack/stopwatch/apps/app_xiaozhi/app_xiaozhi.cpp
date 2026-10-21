/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_xiaozhi.h"
#include <hal/hal.h>
#include <mooncake_log.h>
#include <assets/assets.h>

using namespace mooncake;

AppXiaoZhi::VisibleCallback AppXiaoZhi::s_vis_cb = nullptr;
AppXiaoZhi::ActionCallback AppXiaoZhi::s_start_chat_cb = nullptr;
AppXiaoZhi::ActionCallback AppXiaoZhi::s_stop_chat_cb = nullptr;

void AppXiaoZhi::SetCallbacks(VisibleCallback vis_cb, ActionCallback start_chat_cb, ActionCallback stop_chat_cb) {
    s_vis_cb = std::move(vis_cb);
    s_start_chat_cb = std::move(start_chat_cb);
    s_stop_chat_cb = std::move(stop_chat_cb);
}

AppXiaoZhi::AppXiaoZhi() {
    setAppInfo().name = "XiaoZhi";
    setAppInfo().icon = (void*)&icon_badge;
}

void AppXiaoZhi::onCreate() {
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppXiaoZhi::onOpen() {
    mclog::tagInfo(getAppInfo().name, "on open");
    _key_manager = std::make_unique<input::KeyManager>();

    // 显示小智主界面
    if (s_vis_cb) {
        s_vis_cb(true);
    }
}

void AppXiaoZhi::onRunning() {
    GetHAL().updateButtonStates();
    if (_key_manager && _key_manager->update(false) == input::KeyEvent::GoHome) {
        // 单击 Home 键退出小智，退回 Mooncake 桌面
        close();
        return;
    }
}

void AppXiaoZhi::onClose() {
    mclog::tagInfo(getAppInfo().name, "on close");
    _key_manager.reset();

    // 隐藏小智主界面
    if (s_vis_cb) {
        s_vis_cb(false);
    }
    // 停止语音会话
    if (s_stop_chat_cb) {
        s_stop_chat_cb();
    }
}

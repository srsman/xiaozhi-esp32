/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <apps/common/key_manager/key_manager.h>
#include <mooncake.h>
#include <memory>
#include <functional>

class AppXiaoZhi : public mooncake::AppAbility {
public:
    using VisibleCallback = std::function<void(bool visible)>;
    using ActionCallback = std::function<void()>;

    AppXiaoZhi();

    static void SetCallbacks(VisibleCallback vis_cb, ActionCallback start_chat_cb, ActionCallback stop_chat_cb);

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    std::unique_ptr<input::KeyManager> _key_manager;
    static VisibleCallback s_vis_cb;
    static ActionCallback s_start_chat_cb;
    static ActionCallback s_stop_chat_cb;
};

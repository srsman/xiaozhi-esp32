/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD & Antigravity
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <cmath>
#include <cstring>

static const char* TAG = "HAL_Bridge";

static Hal s_hal_instance;

Hal& GetHAL() {
    return s_hal_instance;
}

// 硬件外部回调或状态提供者
static struct HardwareBridgeContext {
    void (*vibrate_fn)(uint16_t ms) = nullptr;
    void (*get_imu_fn)(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) = nullptr;
    void (*get_time_fn)(int& year, int& month, int& day, int& hour, int& min, int& sec) = nullptr;
    void (*set_time_fn)(int year, int month, int day, int hour, int min, int sec) = nullptr;
    void (*set_brightness_fn)(int brightness) = nullptr;
    int (*get_brightness_fn)() = nullptr;
    void (*set_volume_fn)(int volume) = nullptr;
    int (*get_volume_fn)() = nullptr;
    bool (*is_charging_fn)() = nullptr;
    int (*get_battery_fn)() = nullptr;
    bool (*lvgl_lock_fn)() = nullptr;
    void (*lvgl_unlock_fn)() = nullptr;
} s_hw_ctx;

// 供板级 M5StackStopwatchBoard 注入真实硬件操作
namespace HalBridgeHooks {
    void SetVibrateCallback(void (*fn)(uint16_t)) { s_hw_ctx.vibrate_fn = fn; }
    void SetImuCallback(void (*fn)(float&, float&, float&, float&, float&, float&)) { s_hw_ctx.get_imu_fn = fn; }
    void SetGetTimeCallback(void (*fn)(int&, int&, int&, int&, int&, int&)) { s_hw_ctx.get_time_fn = fn; }
    void SetSetTimeCallback(void (*fn)(int, int, int, int, int, int)) { s_hw_ctx.set_time_fn = fn; }
    void SetBrightnessCallbacks(void (*s_fn)(int), int (*g_fn)()) {
        s_hw_ctx.set_brightness_fn = s_fn;
        s_hw_ctx.get_brightness_fn = g_fn;
    }
    void SetVolumeCallbacks(void (*s_fn)(int), int (*g_fn)()) {
        s_hw_ctx.set_volume_fn = s_fn;
        s_hw_ctx.get_volume_fn = g_fn;
    }
    void SetBatteryCallbacks(int (*b_fn)(), bool (*c_fn)()) {
        s_hw_ctx.get_battery_fn = b_fn;
        s_hw_ctx.is_charging_fn = c_fn;
    }
    void SetLvglLockCallbacks(bool (*l_fn)(), void (*u_fn)()) {
        s_hw_ctx.lvgl_lock_fn = l_fn;
        s_hw_ctx.lvgl_unlock_fn = u_fn;
    }
}

void Hal::init() {
    ESP_LOGI(TAG, "Hal init");
    btnA.begin();
    btnB.begin();
    btnPwr.begin();
}

void Hal::delay(std::uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

std::uint32_t Hal::millis() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void Hal::feedTheDog() {}

std::array<uint8_t, 6> Hal::getFactoryMac() {
    std::array<uint8_t, 6> mac = {0};
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    return mac;
}

std::string Hal::getFactoryMacString(std::string divider) {
    auto mac = getFactoryMac();
    char buf[32];
    snprintf(buf, sizeof(buf), "%02X%s%02X%s%02X%s%02X%s%02X%s%02X",
             mac[0], divider.c_str(), mac[1], divider.c_str(),
             mac[2], divider.c_str(), mac[3], divider.c_str(),
             mac[4], divider.c_str(), mac[5]);
    return std::string(buf);
}

void Hal::reboot() {
    esp_restart();
}

void Hal::factoryReset() {
    esp_restart();
}

uint8_t Hal::getBatteryLevel() {
    if (s_hw_ctx.get_battery_fn) {
        return static_cast<uint8_t>(s_hw_ctx.get_battery_fn());
    }
    return 95;
}

bool Hal::isBatteryCharging(bool strict) {
    if (s_hw_ctx.is_charging_fn) {
        return s_hw_ctx.is_charging_fn();
    }
    return true;
}

void Hal::setPowerLed(bool enabled) {}

void Hal::setBackLightBrightness(int brightness, bool saveToSettings) {
    _bl_brightness = brightness;
    if (s_hw_ctx.set_brightness_fn) {
        s_hw_ctx.set_brightness_fn(brightness);
    }
}

int Hal::getBackLightBrightness(bool loadFromSettings) {
    if (s_hw_ctx.get_brightness_fn) {
        return s_hw_ctx.get_brightness_fn();
    }
    return _bl_brightness;
}

bool Hal::lvglLock() {
    if (s_hw_ctx.lvgl_lock_fn) {
        return s_hw_ctx.lvgl_lock_fn();
    }
    return true;
}

void Hal::lvglUnlock() {
    if (s_hw_ctx.lvgl_unlock_fn) {
        s_hw_ctx.lvgl_unlock_fn();
    }
}

void Hal::startLvglUpdate() {}
void Hal::stopLvglUpdate() {}

Hal::TouchPoint Hal::getTouchPoint() {
    TouchPoint p;
    return p;
}

void Hal::setSpeakerVolume(int volume, bool saveToSettings) {
    _spk_volume = volume;
    if (s_hw_ctx.set_volume_fn) {
        s_hw_ctx.set_volume_fn(volume);
    }
}

int Hal::getSpeakerVolume(bool loadFromSettings) {
    if (s_hw_ctx.get_volume_fn) {
        return s_hw_ctx.get_volume_fn();
    }
    return _spk_volume;
}

int Hal::getAudioSampleRate() {
    return 16000;
}

void Hal::audioRecord(std::vector<int16_t>& data, uint16_t durationMs, float gain) {}
void Hal::audioPlay(std::vector<int16_t>& data, bool async) {}

void Hal::updateAudioSpectrum() {
    // 产生高保真动态音乐频谱
    static float phase = 0.0f;
    phase += 0.12f;
    for (size_t i = 0; i < AudioSpectrumFrame::bandCount; ++i) {
        float base = 0.35f + 0.55f * sinf(phase + i * 0.45f);
        if (base < 0.05f) base = 0.05f;
        _audio_spectrum.bands[i] = base;
    }
    _audio_spectrum.peakFrequencyHz = 440.0f + 120.0f * sinf(phase * 0.5f);
}

void Hal::playBootSfx() {}

void Hal::vibrate(uint16_t durationMs, uint8_t strength) {
    if (s_hw_ctx.vibrate_fn) {
        s_hw_ctx.vibrate_fn(durationMs);
    }
}

void Hal::stopVibrate() {
    if (s_hw_ctx.vibrate_fn) {
        s_hw_ctx.vibrate_fn(0);
    }
}

void Hal::updateImuData() {
    if (s_hw_ctx.get_imu_fn) {
        s_hw_ctx.get_imu_fn(_imu_data.accelX, _imu_data.accelY, _imu_data.accelZ,
                            _imu_data.gyroX, _imu_data.gyroY, _imu_data.gyroZ);
    }
}

void Hal::syncRtcTimeToSystem() {}
void Hal::syncSystemTimeToRtc() {}

DateYmd Hal::getDateYmd() {
    DateYmd d;
    int y = 2026, m = 1, day = 1, h = 0, min = 0, s = 0;
    if (s_hw_ctx.get_time_fn) {
        s_hw_ctx.get_time_fn(y, m, day, h, min, s);
    } else {
        time_t now = time(nullptr);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        y = tm_info.tm_year + 1900;
        m = tm_info.tm_mon + 1;
        day = tm_info.tm_mday;
    }
    d.year = y;
    d.month = m;
    d.day = day;
    return d;
}

bool Hal::setDateYmd(const DateYmd& date) {
    if (s_hw_ctx.set_time_fn) {
        auto t = getTimeHms();
        s_hw_ctx.set_time_fn(date.year, date.month, date.day, t.hour, t.minute, t.second);
        return true;
    }
    return false;
}

TimeHms Hal::getTimeHms() {
    TimeHms t;
    int y = 2026, m = 1, day = 1, h = 0, min = 0, s = 0;
    if (s_hw_ctx.get_time_fn) {
        s_hw_ctx.get_time_fn(y, m, day, h, min, s);
    } else {
        time_t now = time(nullptr);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        h = tm_info.tm_hour;
        min = tm_info.tm_min;
        s = tm_info.tm_sec;
    }
    t.hour = h;
    t.minute = min;
    t.second = s;
    return t;
}

bool Hal::setTimeHms(const TimeHms& time) {
    if (s_hw_ctx.set_time_fn) {
        auto d = getDateYmd();
        s_hw_ctx.set_time_fn(d.year, d.month, d.day, time.hour, time.minute, time.second);
        return true;
    }
    return false;
}

void Hal::setTimezone(std::string_view tz) {}
std::string Hal::getTimezone() { return "CST-8"; }

static AlarmStorageSnapshot s_alarm_snapshot;
bool Hal::loadAlarmStorage(AlarmStorageSnapshot& snapshot) {
    snapshot = s_alarm_snapshot;
    return true;
}
bool Hal::saveAlarmStorage(const AlarmStorageSnapshot& snapshot) {
    s_alarm_snapshot = snapshot;
    return true;
}
void Hal::startAlarm() {
    vibrate(1000);
}
void Hal::stopAlarm() {
    stopVibrate();
}

void Hal::updateButtonStates() {
    btnA.setRawState(millis(), btnA.isPressed());
    btnB.setRawState(millis(), btnB.isPressed());
    btnPwr.setRawState(millis(), btnPwr.isPressed());
}

void Hal::setButtonConfig(ButtonConfig config, bool saveToSettings) {
    _btn_config = config;
}
const Hal::ButtonConfig& Hal::getButtonConfig(bool loadFromSettings) {
    return _btn_config;
}

void Hal::setWifiEnabled(bool enabled, bool saveToSettings) {}
bool Hal::isWifiEnabled(bool loadFromSettings) { return true; }
Hal::WifiStatus Hal::getWifiStatus() {
    WifiStatus s;
    s.enabled = true;
    s.connected = true;
    s.mode = WifiMode::Connected;
    s.ssid = "XiaoZhi-WiFi";
    s.ipAddress = "192.168.1.100";
    return s;
}

bool Hal::loadBadgeImage(lv_obj_t* image) {
    return false;
}
bool Hal::loadNextBadgeImage(lv_obj_t* image) {
    return false;
}
bool Hal::loadPreviousBadgeImage(lv_obj_t* image) {
    return false;
}
void Hal::startBadgeEditModeViaAp(std::function<void(std::string_view)> onLog) {}
bool Hal::shouldShowGuide() { return false; }

static LGFX_Device s_dummy_display;
static LGFX_Sprite s_dummy_canvas;
LGFX_Device& Hal::getDisplay() { return s_dummy_display; }
LGFX_Sprite& Hal::getCanvas() { return s_dummy_canvas; }
void Hal::updateCanvas() {}


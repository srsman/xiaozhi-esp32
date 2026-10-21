/*
 * FluidBox integration for the M5Stack StopWatch demo.
 *
 * The simulation core is adapted from Flowduino/STOPWATCH-FluidBox.
 */
#include "app_fluidbox.h"
#include "FluidBoxConfig.h"

#include <assets/assets.h>
#include <hal/hal.h>
#include <mooncake_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <new>

namespace cfg = fluidbox_config;

namespace {

constexpr int64_t periodMicros(uint16_t rate)
{
    return 1000000LL / rate;
}

}  // namespace

AppFluidBox::AppFluidBox()
{
    setAppInfo().name = "FluidBox";
    setAppInfo().icon = (void*)&fluidbox_icon;
}

void AppFluidBox::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppFluidBox::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _key_manager = std::make_unique<input::KeyManager>();

    LvglLockGuard lock;

    // 创建全屏纯黑底色面板
    _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(466, 466);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setPaddingAll(0);
    _panel->setBgColor(lv_color_black());
    _panel->setBgOpa(LV_OPA_COVER);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _canvas_w = 466;
    _canvas_h = 466;
    _canvas_buf = (uint16_t*)heap_caps_malloc(_canvas_w * _canvas_h * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_canvas_buf) {
        // 如果 466x466 分配不到，回退到 320x320
        _canvas_w = 320;
        _canvas_h = 320;
        _canvas_buf = (uint16_t*)heap_caps_malloc(_canvas_w * _canvas_h * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (_canvas_buf) {
        memset(_canvas_buf, 0, _canvas_w * _canvas_h * sizeof(uint16_t));
        _canvas = lv_canvas_create(_panel->get());
        lv_canvas_set_buffer(_canvas, _canvas_buf, _canvas_w, _canvas_h, LV_COLOR_FORMAT_RGB565);
        lv_obj_center(_canvas);
    }

    _fluid.reset(new (std::nothrow) FluidBox());
    _fluid_ready = _fluid && _fluid->begin(_canvas_w, _canvas_h);
    if (!_fluid) {
        mclog::tagError(getAppInfo().name, "failed to allocate FluidBox solver");
    }

    const int64_t now = esp_timer_get_time();
    _next_imu_micros = now;
    _next_simulation_micros = now;
    _next_render_micros = now;
    _last_imu_micros = 0;
    _touch_down = false;

    mclog::tagInfo(getAppInfo().name, "fluid ready: {}, canvas: {}x{}",
                   _fluid_ready, _canvas_w, _canvas_h);
}

void AppFluidBox::onRunning()
{
    GetHAL().updateButtonStates();
    const auto event = _key_manager ? _key_manager->update(false) : input::KeyEvent::None;

    if (event == input::KeyEvent::GoHome) {
        close();
        return;
    }

    // 按下另一按键重置流体
    if (event == input::KeyEvent::GoNext && _fluid_ready) {
        _fluid->reset();
    }

    updateTouch();

    int64_t now = esp_timer_get_time();

    if (_fluid_ready && now >= _next_imu_micros) {
        _next_imu_micros = now + periodMicros(cfg::kImuHz);
        updateImu(now);
    }

    now = esp_timer_get_time();
    if (_fluid_ready && now >= _next_simulation_micros) {
        _next_simulation_micros = now + periodMicros(cfg::kSimulationHz);
        _fluid->step(1.0f / static_cast<float>(cfg::kSimulationHz));
    }

    now = esp_timer_get_time();
    if (_fluid_ready && now >= _next_render_micros) {
        _next_render_micros = now + periodMicros(cfg::kRenderHz);
        renderFrame();
    }
}

void AppFluidBox::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    _fluid_ready = false;
    _touch_down = false;
    _fluid.reset();
    _key_manager.reset();

    LvglLockGuard lock;
    if (_canvas) {
        lv_obj_delete(_canvas);
        _canvas = nullptr;
    }
    _panel.reset();

    if (_canvas_buf) {
        free(_canvas_buf);
        _canvas_buf = nullptr;
    }
}

void AppFluidBox::renderFrame()
{
    if (!_fluid_ready || !_canvas || !_canvas_buf) {
        return;
    }

    LvglLockGuard lock;
    _fluid->renderToBuffer(_canvas_buf, _canvas_w, _canvas_h);
    lv_obj_invalidate(_canvas);
}

void AppFluidBox::updateImu(int64_t nowMicros)
{
    GetHAL().updateImuData();
    const auto& imu = GetHAL().getImuData();

    float dt = 1.0f / static_cast<float>(cfg::kImuHz);
    if (_last_imu_micros != 0 && nowMicros > _last_imu_micros) {
        dt = static_cast<float>(nowMicros - _last_imu_micros) * 1.0e-6f;
    }
    _last_imu_micros = nowMicros;

    _fluid->setImuSample(
        imu.accelX,
        imu.accelY,
        imu.accelZ,
        imu.gyroX,
        imu.gyroY,
        imu.gyroZ,
        dt);
}

void AppFluidBox::updateTouch()
{
    const auto touch = GetHAL().getTouchPoint();
    const bool touch_down = touch.num > 0 && touch.x >= 0 && touch.y >= 0;

    if (touch_down && !_touch_down && _fluid_ready) {
        const bool displaced =
            _fluid->applyTouchImpulse(static_cast<float>(touch.x), static_cast<float>(touch.y));
        if (displaced && cfg::kTouchVibrationDurationMs > 0 &&
            cfg::kTouchVibrationStrength > 0) {
            GetHAL().vibrate(cfg::kTouchVibrationDurationMs, cfg::kTouchVibrationStrength);
        }
    }

    _touch_down = touch_down;
}

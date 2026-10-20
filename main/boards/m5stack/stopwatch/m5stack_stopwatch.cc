#include "wifi_board.h"
#include "backlight.h"
#include "display/lcd_display.h"
#include "muse_avatar.h"
#include "esp_lcd_co5300.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "M5IOE1.h"
#include "M5PM1.h"
#include "config.h"
#include "assets/lang_config.h"
#include "watch_apps.h"
#include "rx8130.h"
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <wifi_manager.h>
#include "i2c_bus.h"
#include "bmi270_api.h"

#define TAG "M5StackStopwatch"
#define LCD_OPCODE_WRITE_CMD (0x02ULL)

// CO5300 AMOLED: initialize at full brightness, then restore the saved setting.
static const co5300_lcd_init_cmd_t vendor_specific_init[] = {
    // {cmd, { data }, data_size, delay_ms}
    {0xFE, (uint8_t []){0x00}, 0, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 0, 10}, // RGB565
    {0x35, (uint8_t []){0x00}, 0, 10},
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x51, (uint8_t []){0xFF}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Column address: 0-465
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0xD1}, 4, 0}, // Row address: 0-465
    {0x11, (uint8_t []){0x00}, 0, 120}, // Exit sleep
    {0x29, (uint8_t []){0x00}, 0, 20},  // Display on
};

class RoundLcdDisplay : public SpiLcdDisplay {
private:
    std::unique_ptr<MuseAvatar> muse_avatar_ = nullptr;
    std::function<void()> touch_cb_ = nullptr;

public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = static_cast<lv_area_t*>(lv_event_get_param(e));
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }

    RoundLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

    void SetTouchCallback(std::function<void()> cb) {
        touch_cb_ = std::move(cb);
        if (muse_avatar_) {
            muse_avatar_->OnTouch(touch_cb_);
        }
    }

    MuseAvatar* GetMuseAvatar() {
        return muse_avatar_.get();
    }

    void SetStatus(const char* status) override {
        const char* zh_status = status;
        if (status != nullptr) {
            if (strstr(status, "Standby") || strstr(status, "standby")) {
                zh_status = "待命";
            } else if (strstr(status, "Listening") || strstr(status, "listening")) {
                zh_status = "聆听中...";
            } else if (strstr(status, "Speaking") || strstr(status, "speaking")) {
                zh_status = "说话中...";
            } else if (strstr(status, "Connecting") || strstr(status, "connecting")) {
                zh_status = "连接中...";
            } else if (strstr(status, "Thinking") || strstr(status, "thinking")) {
                zh_status = "思考中...";
            } else if (strstr(status, "Wi-Fi Config") || strstr(status, "WiFi Config") || strstr(status, "Config")) {
                zh_status = "配网模式";
            } else if (strstr(status, "Initializing") || strstr(status, "initializing")) {
                zh_status = "初始化...";
            } else if (strstr(status, "Upgrading") || strstr(status, "upgrading")) {
                zh_status = "升级中...";
            }
        }
        SpiLcdDisplay::SetStatus(zh_status);
        if (!muse_avatar_ || !status) return;

        if (std::strcmp(status, Lang::Strings::CONNECTING) == 0 ||
            strstr(status, "连接") || strstr(status, "思考") || strstr(status, "Think") || strstr(status, "think") ||
            strstr(status, "CONNECTING") || strstr(status, "Connecting")) {
            muse_avatar_->SetMode(MUSE_MODE_THINKING);
        } else if (std::strcmp(status, Lang::Strings::LISTENING) == 0 || strstr(status, "聆听") || strstr(status, "Listen")) {
            muse_avatar_->SetMode(MUSE_MODE_LISTENING);
        } else if (std::strcmp(status, Lang::Strings::SPEAKING) == 0 || strstr(status, "说话") || strstr(status, "Speak")) {
            muse_avatar_->SetMode(MUSE_MODE_SPEAKING);
        } else if (std::strcmp(status, Lang::Strings::STANDBY) == 0 || strstr(status, "待命") || strstr(status, "Standby")) {
            muse_avatar_->SetMode(MUSE_MODE_IDLE);
        }
    }

    void ShowNotification(const std::string& notification, int duration_ms = 3000) override {
        std::string zh_notify = notification;
        if (zh_notify.find("Entering Wi-Fi config mode") != std::string::npos) {
            zh_notify = "进入配网模式...";
        } else if (zh_notify.find("Volume:") != std::string::npos) {
            zh_notify = "音量: " + zh_notify.substr(zh_notify.find("Volume:") + 7);
        }
        SpiLcdDisplay::ShowNotification(zh_notify, duration_ms);
    }

    void SetChatMessage(const char* role, const char* content) override {
        std::string zh_content = content ? content : "";
        if (zh_content.find("Connect to AP:") != std::string::npos ||
            zh_content.find("Connect to hotspot") != std::string::npos ||
            zh_content.find("Open browser to:") != std::string::npos) {
            std::string ssid = WifiManager::GetInstance().GetApSsid();
            std::string url = WifiManager::GetInstance().GetApWebUrl();
            zh_content = "手机连接热点 " + ssid + "\n浏览器访问 " + url;
        }
        SpiLcdDisplay::SetChatMessage(role, zh_content.c_str());
    }

    void SetEmotion(const char* emotion) override {
        SpiLcdDisplay::SetEmotion(emotion);
        DisplayLockGuard lock(this);
        // 保持基类原有静态组件隐藏，由 Muse Avatar 独占渲染
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }

        if (!muse_avatar_ || !emotion) return;
        if (strstr(emotion, "thinking")) {
            muse_avatar_->SetMode(MUSE_MODE_THINKING);
        } else {
            muse_avatar_->SetEmotion(emotion);
        }
    }

    void SetTheme(Theme* theme) override {
        // 先调用基类更新字体、图标等必要属性
        SpiLcdDisplay::SetTheme(theme);
        DisplayLockGuard lock(this);

        // 强行锁定全屏纯黑 AMOLED，彻底阻断任何联网激活、配置下发或 light 主题将背景刷白
        lv_obj_t* scr = lv_screen_active();
        if (scr != nullptr) {
            lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(scr, lv_color_hex(0xffffff), 0);
        }

        if (container_ != nullptr) {
            lv_obj_set_style_bg_image_src(container_, nullptr, 0);
            lv_obj_set_style_bg_color(container_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(container_, 0, 0);
        }

        if (status_bar_ != nullptr) {
            lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
        }
        if (bottom_bar_ != nullptr) {
            lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(bottom_bar_, 0, 0);
        }
        if (top_bar_ != nullptr) {
            lv_obj_set_style_bg_color(top_bar_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(top_bar_, 0, 0);
        }
        if (emoji_box_ != nullptr) {
            lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(emoji_box_, 0, 0);
        }

        // 确保所有文字始终维持纯白与冷光浅青
        if (status_label_ != nullptr) {
            lv_obj_set_style_text_color(status_label_, lv_color_hex(0xffffff), 0);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_style_text_color(notification_label_, lv_color_hex(0x38bdf8), 0);
        }
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0xf1f5f9), 0);
        }
        if (network_label_ != nullptr) {
            lv_obj_set_style_text_color(network_label_, lv_color_hex(0xffffff), 0);
        }
        if (battery_label_ != nullptr) {
            lv_obj_set_style_text_color(battery_label_, lv_color_hex(0xffffff), 0);
        }

        // 保持最外层流光脉冲环置顶
        if (muse_avatar_ && muse_avatar_->GetRingObject()) {
            lv_obj_move_foreground(muse_avatar_->GetRingObject());
        }
    }

    void SetupUI() override {
        SpiLcdDisplay::SetupUI();
        DisplayLockGuard lock(this);

        // 1. 全屏纯黑沉浸化（彻底消除白色大背景与方块边界）
        lv_obj_t* scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(scr, lv_color_hex(0xffffff), 0);

        // 核心关键：彻底将父类创建的全屏白色 container_ 设为全黑，绝无白底白边
        if (container_ != nullptr) {
            lv_obj_set_style_bg_color(container_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(container_, 0, 0);
        }

        if (status_bar_ != nullptr) {
            lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
        }
        if (bottom_bar_ != nullptr) {
            lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(bottom_bar_, 0, 0);
        }
        if (top_bar_ != nullptr) {
            lv_obj_set_style_bg_color(top_bar_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(top_bar_, 0, 0);
        }
        if (emoji_box_ != nullptr) {
            lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(emoji_box_, 0, 0);
        }

        // 统一所有文字为纯白与冷光浅青
        if (status_label_ != nullptr) {
            lv_obj_set_style_text_color(status_label_, lv_color_hex(0xffffff), 0);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_style_text_color(notification_label_, lv_color_hex(0x38bdf8), 0);
        }
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0xf1f5f9), 0);
        }
        if (network_label_ != nullptr) {
            lv_obj_set_style_text_color(network_label_, lv_color_hex(0xffffff), 0);
        }
        if (battery_label_ != nullptr) {
            lv_obj_set_style_text_color(battery_label_, lv_color_hex(0xffffff), 0);
        }

        // 隐藏默认静态文字与图片
        if (emoji_label_ != nullptr) {
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
        }
        if (emoji_image_ != nullptr) {
            lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }

        // 2. 初始化 Meta Muse 核心 Avatar 与环形脉冲渐变外环 (280px 居中 + 466px 圆屏外径)
        muse_avatar_ = std::make_unique<MuseAvatar>(emoji_box_, 280, DISPLAY_WIDTH);
        if (touch_cb_) {
            muse_avatar_->OnTouch(touch_cb_);
        }

        // 3. 圆屏文字布局适配
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES * 0.2, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.2, 0);
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_STATUS_BAR_TOP_OFF);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.6);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.6);
        lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);

        if (bottom_bar_ != nullptr) {
            lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, -DISPLAY_CHAT_BAR_BOTTOM_OFF);
            lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75);
        }

        if (top_bar_ != nullptr) {
            lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, DISPLAY_ROUND_EDGE_INSET / 2);
            lv_obj_set_style_pad_top(top_bar_, DISPLAY_ROUND_EDGE_INSET / 4, 0);
        }

        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class StopwatchBacklight : public Backlight {
public:
    StopwatchBacklight(esp_lcd_panel_io_handle_t panel_io, Display* display)
        : panel_io_(panel_io), display_(display) {}

protected:
    void SetBrightnessImpl(uint8_t brightness) override {
        DisplayLockGuard lock(display_);
        uint8_t data[] = {static_cast<uint8_t>((255 * brightness) / 100)};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;

        esp_err_t err = esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, data, sizeof(data));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set display brightness: %s", esp_err_to_name(err));
        }
    }

private:
    esp_lcd_panel_io_handle_t panel_io_;
    Display* display_;
};

class M5StackStopwatchBoard : public WifiBoard {
private:
    i2c_bus_handle_t shared_i2c_bus_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    M5PM1 pmic_;
    M5IOE1 ioe_;
    Button button1_;
    Button button2_;
    RoundLcdDisplay* display_;
    StopwatchBacklight* backlight_;

    void InitializeI2c() {
        i2c_config_t i2c_bus_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {
                .clk_speed = 400000,
            },
            .clk_flags = 0,
        };
        shared_i2c_bus_ = i2c_bus_create(I2C_NUM_0, &i2c_bus_cfg);
        if (!shared_i2c_bus_) {
            ESP_LOGE(TAG, "Failed to create shared I2C bus");
            return;
        }
        i2c_bus_ = i2c_bus_get_internal_bus_handle(shared_i2c_bus_);

        if (ioe_.begin(i2c_bus_, M5IOE1_I2C_ADDR, M5IOE1_I2C_FREQ_100K, M5IOE1_INT_MODE_POLLING) != M5IOE1_OK) {
            ESP_LOGE(TAG, "M5IOE1 begin failed");
            return;
        }

        if (pmic_.begin(i2c_bus_, M5PM1_DEFAULT_ADDR, M5PM1_I2C_FREQ_100K) != M5PM1_OK) {
            ESP_LOGE(TAG, "M5PM1 begin failed");
            return;
        }

        pmic_.setChargeEnable(true);
        pmic_.setBoostEnable(true);
        pmic_.pinMode(PMIC_PIN_CHARGE_STATE, INPUT);
        pmic_.pinMode(PMIC_PIN_CHARGE_PROG, OUTPUT);
        pmic_.digitalWrite(PMIC_PIN_CHARGE_PROG, LOW); // Set charge current to 425mA
        pmic_.setLedEnLevel(false); // Turn off distracting green power LED while AMOLED is ON

        ioe_.pinMode(IOE_PIN_LCD_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_LCD_RST, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_LCD_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);

        ioe_.pinMode(IOE_PIN_CODEC_POWER, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_CODEC_POWER, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_CODEC_POWER, HIGH);

        ioe_.pinMode(IOE_PIN_PA_EN, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_PA_EN, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_PA_EN, HIGH);

        ioe_.pinMode(IOE_PIN_MOTOR, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_MOTOR, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);

        // Power on and release Touch controller reset (CST820B)
        ioe_.pinMode(IOE_PIN_TOUCH_RST, OUTPUT);
        ioe_.setDriveMode(IOE_PIN_TOUCH_RST, M5IOE1_DRIVE_PUSHPULL);
        ioe_.digitalWrite(IOE_PIN_TOUCH_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE_PIN_TOUCH_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(50));

        vTaskDelay(pdMS_TO_TICKS(100));

        // Initialize RX8130 Hardware RTC
        Rx8130Rtc::GetInstance().Init(i2c_bus_);
    }

    bmi270_handle_t bmi_handle_ = nullptr;
    TaskHandle_t imu_task_handle_ = nullptr;

    void InitializeImu() {
        if (!shared_i2c_bus_) return;

        esp_err_t ret = bmi270_sensor_create(shared_i2c_bus_, &bmi_handle_, bmi270_config_file,
                                             BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE);
        if (ret != ESP_OK || !bmi_handle_) {
            ESP_LOGW(TAG, "BMI270 create failed: %s", esp_err_to_name(ret));
            return;
        }

        const uint8_t sens_list[] = {BMI2_ACCEL};
        int8_t rslt = bmi270_sensor_enable(sens_list, 1, bmi_handle_);
        if (rslt != BMI2_OK) {
            ESP_LOGW(TAG, "BMI270 accel enable failed: %d", rslt);
            return;
        }

        ESP_LOGI(TAG, "BMI270 IMU initialized successfully with official microcode!");

        xTaskCreate([](void* arg) {
            auto* self = static_cast<M5StackStopwatchBoard*>(arg);
            if (!self || !self->bmi_handle_) {
                vTaskDelete(NULL);
                return;
            }

            int16_t prev_x = 0, prev_y = 0, prev_z = 0;
            bool has_prev = false;
            int64_t last_shake_tick = 0;

            while (true) {
                struct bmi2_sens_data sens_data;
                if (bmi2_get_sensor_data(&sens_data, self->bmi_handle_) == BMI2_OK) {
                    int16_t cur_x = sens_data.acc.x;
                    int16_t cur_y = sens_data.acc.y;
                    int16_t cur_z = sens_data.acc.z;

                    if (has_prev) {
                        int dx = std::abs((int)cur_x - (int)prev_x);
                        int dy = std::abs((int)cur_y - (int)prev_y);
                        int dz = std::abs((int)cur_z - (int)prev_z);
                        int shake_score = dx + dy + dz;

                        int64_t now_ms = esp_timer_get_time() / 1000;
                        if (shake_score > 18000 && (now_ms - last_shake_tick) > 2000) {
                            last_shake_tick = now_ms;
                            ESP_LOGI(TAG, "Shake detected (score: %d), trigger dizzy reaction", shake_score);

                            if (self->display_ && self->display_->GetMuseAvatar()) {
                                self->display_->GetMuseAvatar()->TriggerAction(MUSE_ACTION_DIZZY, 2.5f);
                                self->ioe_.digitalWrite(IOE_PIN_MOTOR, HIGH);
                                vTaskDelay(pdMS_TO_TICKS(80));
                                self->ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);
                            }
                        }
                    }

                    // Feed IMU acceleration to WatchPageManager to drive FluidBox
                    // The StopWatch display swaps X/Y
                    WatchPageManager::GetInstance().SetImuAccel(
                        (float)cur_y / 16384.0f * 9.8f,
                        (float)cur_x / 16384.0f * 9.8f,
                        (float)cur_z / 16384.0f * 9.8f
                    );

                    prev_x = cur_x;
                    prev_y = cur_y;
                    prev_z = cur_z;
                    has_prev = true;
                }
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }, "imu_task", 4096, this, 3, &imu_task_handle_);
    }

    i2c_master_dev_handle_t touch_dev_ = nullptr;
    lv_indev_t* touch_indev_ = nullptr;

    void InitializeTouch() {
        if (!i2c_bus_ || !display_) return;
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x15, // CST820B I2C address
            .scl_speed_hz = 100000,
        };
        esp_err_t err = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &touch_dev_);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Touch device add failed: %s", esp_err_to_name(err));
            return;
        }

        DisplayLockGuard lock(display_);
        touch_indev_ = lv_indev_create();
        if (!touch_indev_) return;
        lv_indev_set_type(touch_indev_, LV_INDEV_TYPE_POINTER);
        lv_indev_set_user_data(touch_indev_, this);
        lv_indev_set_read_cb(touch_indev_, [](lv_indev_t* indev, lv_indev_data_t* data) {
            data->state = LV_INDEV_STATE_RELEASED;
            auto* self = static_cast<M5StackStopwatchBoard*>(lv_indev_get_user_data(indev));
            if (!self || !self->touch_dev_) return;

            uint8_t reg = 0x01;
            uint8_t read_buf[6] = {0};
            esp_err_t r_err = i2c_master_transmit_receive(self->touch_dev_, &reg, 1, read_buf, sizeof(read_buf), 10);
            if (r_err == ESP_OK && read_buf[1] > 0) { // points count > 0 at reg 0x02
                uint16_t x = ((read_buf[2] & 0x0F) << 8) | read_buf[3];
                uint16_t y = ((read_buf[4] & 0x0F) << 8) | read_buf[5];
                if (x < DISPLAY_WIDTH && y < DISPLAY_HEIGHT) {
                    data->point.x = x;
                    data->point.y = y;
                    data->state = LV_INDEV_STATE_PRESSED;
                }
            }
        });
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = DISPLAY_QSPI_SCK;
        buscfg.data0_io_num = DISPLAY_QSPI_D0;
        buscfg.data1_io_num = DISPLAY_QSPI_D1;
        buscfg.data2_io_num = DISPLAY_QSPI_D2;
        buscfg.data3_io_num = DISPLAY_QSPI_D3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeDisplay() {
        ioe_.digitalWrite(IOE_PIN_LCD_RST, LOW);
        vTaskDelay(pdMS_TO_TICKS(10));
        ioe_.digitalWrite(IOE_PIN_LCD_RST, HIGH);
        vTaskDelay(pdMS_TO_TICKS(120));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_QSPI_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_QSPI_HOST, &io_config, &panel_io));

        co5300_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(co5300_lcd_init_cmd_t),
            .flags = {.use_qspi_interface = 1},
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(panel_io, &panel_config, &panel));

        esp_lcd_panel_set_gap(panel, 0x06, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new RoundLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                       DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                       DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        display_->SetTouchCallback([this]() {
            ioe_.digitalWrite(IOE_PIN_MOTOR, HIGH);
            vTaskDelay(pdMS_TO_TICKS(50));
            ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);
        });
        backlight_ = new StopwatchBacklight(panel_io, display_);
        backlight_->RestoreBrightness();
    }

    void InitializeButtons() {
        // Button1: 如果在子页面（表盘/秒表），充当 Home 键返回小智主屏；在主屏时唤醒/切换对话
        button1_.OnClick([this]() {
            if (WatchPageManager::GetInstance().HandleBack()) {
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiManager::GetInstance().IsConnected()) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // Button2: 待机时循环切换 12 个互动动作，对话时调音量
        button2_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                if (display_ && display_->GetMuseAvatar()) {
                    static int s_action_idx = 0;
                    static const muse_action_t ACTIONS[] = {
                        MUSE_ACTION_HAPPY,
                        MUSE_ACTION_WAVE,
                        MUSE_ACTION_POKE,
                        MUSE_ACTION_EXCITED,
                        MUSE_ACTION_KISS,
                        MUSE_ACTION_SHY,
                        MUSE_ACTION_SNEEZE,
                        MUSE_ACTION_SULKY,
                        MUSE_ACTION_DIZZY,
                        MUSE_ACTION_SHOCKED,
                        MUSE_ACTION_SAD,
                        MUSE_ACTION_SLEEPY,
                    };
                    display_->GetMuseAvatar()->TriggerAction(ACTIONS[s_action_idx], 2.2f);
                    s_action_idx = (s_action_idx + 1) % 12;
                    ioe_.digitalWrite(IOE_PIN_MOTOR, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(40));
                    ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);
                    return;
                }
            }
            auto* codec = GetAudioCodec();
            int volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ":" + std::to_string(volume) + "%");
        });
    }

public:
    M5StackStopwatchBoard()
        : shared_i2c_bus_(nullptr),
          i2c_bus_(nullptr),
          button1_(BUTTON1_GPIO),
          button2_(BUTTON2_GPIO),
          display_(nullptr),
          backlight_(nullptr) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializeImu();

        if (display_) {
            DisplayLockGuard lock(display_);
            WatchPageManager::GetInstance().Init(
                lv_screen_active(),
                [this](uint16_t ms) {
                    ioe_.digitalWrite(IOE_PIN_MOTOR, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(ms));
                    ioe_.digitalWrite(IOE_PIN_MOTOR, LOW);
                },
                [this]() -> int {
                    int lvl = 100;
                    bool chg = false, dis = false;
                    GetBatteryLevel(lvl, chg, dis);
                    return lvl;
                },
                [this](uint8_t br) {
                    if (backlight_) backlight_->SetBrightness(br);
                },
                [this](uint8_t vol) {
                    auto* codec = GetAudioCodec();
                    if (codec) codec->SetOutputVolume(vol);
                }
            );
        }
    }

    AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_GPIO_PA,
            AUDIO_CODEC_ES8311_ADDR,
            false);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        return backlight_;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        uint16_t voltage_mv = 0;
        if (pmic_.readVbat(&voltage_mv) != M5PM1_OK) {
            return false;
        }

        int charge_state_level = pmic_.digitalRead(PMIC_PIN_CHARGE_STATE);
        if (charge_state_level < 0) {
            // Charge status read failed; leave charging/discharging unknown
            charging = false;
            discharging = false;
        } else {
            // M5PM1 charge status is active low: 0 means charging, 1 means not charging
            charging = (charge_state_level == 0);
            discharging = !charging;
        }

        const int BATTERY_MIN_VOLTAGE = 3400;
        const int BATTERY_MAX_VOLTAGE = 4200;
        if (voltage_mv < BATTERY_MIN_VOLTAGE) {
            level = 0;
        } else if (voltage_mv > BATTERY_MAX_VOLTAGE) {
            level = 100;
        } else {
            level = ((voltage_mv - BATTERY_MIN_VOLTAGE) * 100) / (BATTERY_MAX_VOLTAGE - BATTERY_MIN_VOLTAGE);
        }
        return true;
    }
};

DECLARE_BOARD(M5StackStopwatchBoard);

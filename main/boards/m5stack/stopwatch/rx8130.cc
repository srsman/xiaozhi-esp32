#include "rx8130.h"
#include <esp_log.h>
#include <sys/time.h>

#define TAG "RX8130"

#define RX8130_REG_SEC   0x10
#define RX8130_REG_CTRL0 0x1E
#define RX8130_REG_CTRL1 0x1F
#define RX8130_BIT_CTRL_STOP (1 << 6)

Rx8130Rtc& Rx8130Rtc::GetInstance() {
    static Rx8130Rtc instance;
    return instance;
}

bool Rx8130Rtc::Init(i2c_master_bus_handle_t i2c_bus, uint8_t addr) {
    if (!i2c_bus) return false;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev_);
    if (err != ESP_OK || !dev_) {
        ESP_LOGW(TAG, "RTC add device failed: %s", esp_err_to_name(err));
        return false;
    }

    // Enable backup battery charging
    uint8_t reg = RX8130_REG_CTRL1;
    uint8_t ctrl1 = 0;
    if (i2c_master_transmit_receive(dev_, &reg, 1, &ctrl1, 1, 50) == ESP_OK) {
        ctrl1 |= 0b00110000;
        uint8_t write_buf[2] = {RX8130_REG_CTRL1, ctrl1};
        i2c_master_transmit(dev_, write_buf, sizeof(write_buf), 50);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "RX8130 RTC initialized");

    // Automatically sync RTC time to system on boot
    SyncToSystemTime();
    return true;
}

bool Rx8130Rtc::GetTime(struct tm* time_out) {
    if (!initialized_ || !dev_ || !time_out) return false;
    uint8_t reg = RX8130_REG_SEC;
    uint8_t date[7] = {0};
    if (i2c_master_transmit_receive(dev_, &reg, 1, date, 7, 50) != ESP_OK) {
        return false;
    }

    time_out->tm_sec  = BcdToDec(date[0] & 0x7F);
    time_out->tm_min  = BcdToDec(date[1] & 0x7F);
    time_out->tm_hour = BcdToDec(date[2] & 0x3F);
    time_out->tm_wday = BcdToDec(date[3] & 0x07);
    time_out->tm_mday = BcdToDec(date[4] & 0x3F);
    int rtc_month     = BcdToDec(date[5] & 0x1F);
    time_out->tm_mon  = (rtc_month >= 1 && rtc_month <= 12) ? (rtc_month - 1) : 0;
    time_out->tm_year = BcdToDec(date[6]) + 100; // 2000 + year -> tm_year is years since 1900

    return true;
}

bool Rx8130Rtc::SetTime(const struct tm* time_in) {
    if (!initialized_ || !dev_ || !time_in) return false;

    // Set STOP bit before updating
    uint8_t reg = RX8130_REG_CTRL0;
    uint8_t ctrl0 = 0;
    i2c_master_transmit_receive(dev_, &reg, 1, &ctrl0, 1, 50);
    uint8_t stop_cmd[2] = {RX8130_REG_CTRL0, (uint8_t)(ctrl0 | RX8130_BIT_CTRL_STOP)};
    i2c_master_transmit(dev_, stop_cmd, 2, 50);

    uint8_t write_buf[8] = {
        RX8130_REG_SEC,
        DecToBcd(time_in->tm_sec),
        DecToBcd(time_in->tm_min),
        DecToBcd(time_in->tm_hour),
        DecToBcd(time_in->tm_wday),
        DecToBcd(time_in->tm_mday),
        DecToBcd(time_in->tm_mon + 1),
        DecToBcd((time_in->tm_year >= 100) ? (time_in->tm_year - 100) : time_in->tm_year)
    };
    i2c_master_transmit(dev_, write_buf, sizeof(write_buf), 50);

    // Clear STOP bit
    uint8_t start_cmd[2] = {RX8130_REG_CTRL0, (uint8_t)(ctrl0 & ~RX8130_BIT_CTRL_STOP)};
    i2c_master_transmit(dev_, start_cmd, 2, 50);

    return true;
}

bool Rx8130Rtc::SyncToSystemTime() {
    struct tm t;
    if (!GetTime(&t)) return false;

    // Sanity check: year must be >= 2024
    if (t.tm_year < 124) {
        ESP_LOGW(TAG, "RTC time invalid or uninitialized (year < 2024)");
        return false;
    }

    time_t epoch = mktime(&t);
    if (epoch <= 0) return false;

    struct timeval tv = {.tv_sec = epoch, .tv_usec = 0};
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "System time synced from RTC: %04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return true;
}

bool Rx8130Rtc::SyncFromSystemTime() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year < 124) return false; // Don't save uncalibrated system time
    return SetTime(&t);
}

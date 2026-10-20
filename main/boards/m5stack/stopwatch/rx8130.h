#ifndef RX8130_H
#define RX8130_H

#include <driver/i2c_master.h>
#include <time.h>
#include <stdbool.h>

class Rx8130Rtc {
public:
    static Rx8130Rtc& GetInstance();

    bool Init(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x32);
    bool GetTime(struct tm* time_out);
    bool SetTime(const struct tm* time_in);
    bool SyncToSystemTime();
    bool SyncFromSystemTime();

private:
    Rx8130Rtc() = default;
    i2c_master_dev_handle_t dev_ = nullptr;
    bool initialized_ = false;

    static uint8_t BcdToDec(uint8_t val) {
        return (val >> 4) * 10 + (val & 0x0F);
    }
    static uint8_t DecToBcd(uint8_t val) {
        return ((val / 10) << 4) + (val % 10);
    }
};

#endif // RX8130_H

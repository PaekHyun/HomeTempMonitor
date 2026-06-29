/*
 * RTClib_DS3231.h - Minimal DS3231 RTC driver (I2C)
 * 
 * Only the functions needed for this project:
 *   - Read current time
 *   - Set time from NTP
 *   - Read temperature (on-chip sensor)
 */
#ifndef RTCLIB_DS3231_H
#define RTCLIB_DS3231_H

#include <Arduino.h>
#include <Wire.h>

// DS3231 I2C address (fixed)
#define DS3231_ADDR 0x68

struct DateTime {
  uint16_t year;   // 2000-2099
  uint8_t  month;  // 1-12
  uint8_t  day;    // 1-31
  uint8_t  hour;   // 0-23
  uint8_t  minute; // 0-59
  uint8_t  second; // 0-59
  uint8_t  dow;    // 1=Mon..7=Sun

  uint32_t unixtime() const;
  void     fromUnixTime(uint32_t t);
};

class DS3231_RTC {
public:
  bool begin(TwoWire *wire = &Wire);

  DateTime now();
  void     adjust(const DateTime &dt);
  float    getTemperature();  // DS3231 built-in temp sensor

  bool     lostPower();       // Check if oscillator stopped (power loss)
  void     clearLostPower();

private:
  TwoWire *_wire;
  uint8_t  bcd2bin(uint8_t val) { return val - 6 * (val >> 4); }
  uint8_t  bin2bcd(uint8_t val) { return val + 6 * (val / 10); }
};

#endif // RTCLIB_DS3231_H

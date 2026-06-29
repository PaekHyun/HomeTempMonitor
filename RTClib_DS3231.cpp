/*
 * RTClib_DS3231.cpp - Minimal DS3231 RTC driver implementation
 */
#include "RTClib_DS3231.h"

// DS3231 Registers
#define REG_TIME       0x00  // Seconds, Minutes, Hours, Day, Date, Month/Century, Year
#define REG_ALARM1     0x07
#define REG_ALARM2    0x0B
#define REG_CONTROL   0x0E
#define REG_STATUS    0x0F
#define REG_AGING     0x10
#define REG_TEMP_MSB  0x11
#define REG_TEMP_LSB  0x12

// ── DateTime methods ──
uint32_t DateTime::unixtime() const {
  // Days up to month for non-leap and leap years
  static const uint8_t daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  
  uint32_t days = 0;
  // Years since 2000
  for (uint16_t y = 2000; y < year; y++) {
    days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  }
  // Months this year
  for (uint8_t m = 1; m < month; m++) {
    days += daysInMonth[m - 1];
    if (m == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) {
      days += 1; // Leap year February
    }
  }
  days += day - 1;

  // Unix epoch offset: days from 2000-01-01 to 1970-01-01 = 10957
  uint32_t t = (days + 10957) * 86400UL;
  t += (uint32_t)hour * 3600;
  t += (uint32_t)minute * 60;
  t += second;
  return t;
}

void DateTime::fromUnixTime(uint32_t t) {
  // Convert Unix timestamp to date/time components
  t -= 946684800UL; // Offset from 1970 to 2000

  second = t % 60;
  t /= 60;
  minute = t % 60;
  t /= 60;
  hour = t % 24;
  uint32_t days = t / 24;

  // Calculate year
  year = 2000;
  while (true) {
    uint16_t daysInYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
    if (days < daysInYear) break;
    days -= daysInYear;
    year++;
  }

  // Calculate month
  static const uint8_t daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  month = 1;
  for (uint8_t m = 0; m < 12; m++) {
    uint8_t dim = daysInMonth[m];
    if (m == 1 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) dim++;
    if (days < dim) break;
    days -= dim;
    month++;
  }

  day = days + 1;
  dow = 1; // Not calculated (not needed for this project)
}

// ── DS3231_RTC methods ──
bool DS3231_RTC::begin(TwoWire *wire) {
  _wire = wire;
  _wire->beginTransmission(DS3231_ADDR);
  return (_wire->endTransmission() == 0);
}

DateTime DS3231_RTC::now() {
  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_TIME);
  _wire->endTransmission();

  _wire->requestFrom(DS3231_ADDR, (uint8_t)7);
  uint8_t ss = bcd2bin(_wire->read() & 0x7F);
  uint8_t mm = bcd2bin(_wire->read());
  uint8_t hh = bcd2bin(_wire->read());
  _wire->read(); // Day of week (skip)
  uint8_t d  = bcd2bin(_wire->read());
  uint8_t mo = bcd2bin(_wire->read() & 0x7F);
  uint16_t y = bcd2bin(_wire->read()) + 2000;

  return DateTime{y, mo, d, hh, mm, ss, 0};
}

void DS3231_RTC::adjust(const DateTime &dt) {
  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_TIME);
  _wire->write(bin2bcd(dt.second));
  _wire->write(bin2bcd(dt.minute));
  _wire->write(bin2bcd(dt.hour));
  _wire->write(bin2bcd(dt.dow));    // Day of week
  _wire->write(bin2bcd(dt.day));
  _wire->write(bin2bcd(dt.month));
  _wire->write(bin2bcd(dt.year - 2000));
  _wire->endTransmission();

  clearLostPower();
}

float DS3231_RTC::getTemperature() {
  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_TEMP_MSB);
  _wire->endTransmission();

  _wire->requestFrom(DS3231_ADDR, (uint8_t)2);
  int8_t msb = _wire->read();
  uint8_t lsb = _wire->read();

  return (float)msb + (lsb >> 6) * 0.25f;
}

bool DS3231_RTC::lostPower() {
  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_STATUS);
  _wire->endTransmission();

  _wire->requestFrom(DS3231_ADDR, (uint8_t)1);
  uint8_t status = _wire->read();
  return (status & 0x80) != 0; // OSF bit
}

void DS3231_RTC::clearLostPower() {
  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_STATUS);
  _wire->endTransmission();

  _wire->requestFrom(DS3231_ADDR, (uint8_t)1);
  uint8_t status = _wire->read();

  _wire->beginTransmission(DS3231_ADDR);
  _wire->write(REG_STATUS);
  _wire->write(status & 0x7F); // Clear OSF bit
  _wire->endTransmission();
}

/*
 * ═══════════════════════════════════════════════════════════════
 *  Home Temperature & Humidity Monitor
 *  XIAO ESP32C6 + SHT40 + 2.9" E-Paper + DS3231
 *  (내장 플래시 LittleFS 저장)
 * ═══════════════════════════════════════════════════════════════
 *
 * Features:
 *  ✅ SHT40 sensor → temperature & humidity (I2C)
 *  ✅ 2.9" BWR e-paper → real-time display (SPI, SSD1680)
 *  ✅ 내장 플래시 LittleFS → CSV 데이터 저장
 *  ✅ DS3231 RTC → 정확한 타임스탬프 (I2C)
 *  ✅ WiFi NTP → 24시간마다 시간 동기화
 *  ✅ Web server → 브라우저에서 CSV 다운로드
 *  ✅ Deep sleep → 초저전력 (~15uA)
 *  ✅ Min/Max 추적 (RTC 메모리 유지)
 *
 * Required Libraries (Arduino Library Manager):
 *  - GxEPD2 by Jean-Marc Capello
 *  - Adafruit SHT4x Library
 *  - Adafruit GFX Library (GxEPD2 dependency)
 *
 * Board Package:
 *  - esp32 by Espressif v3.0.0+ (ESP32C6 support)
 *  - Board: "XIAO_ESP32C6"
 *
 * 파티션 설정:
 *  - Tools → Partition Scheme →
 *    "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
 *    또는 커스텀 파티션 테이블 사용
 */

// ===================== INCLUDES =====================
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Adafruit_SHT4x.h>
#include <SPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>

#include "DataLogger.h"
#include "RTClib_DS3231.h"
#include "WebServer.h"

// ===================== CONFIGURATION =====================
#define SLEEP_INTERVAL_MIN       5       // Deep sleep interval (minutes)
#define WIFI_SYNC_INTERVAL_HR    24      // NTP sync every N hours
#define WIFI_AP_TIMEOUT_SEC      120     // Web server active duration (seconds)
#define SERIAL_BAUD              115200
#define TIMEZONE_OFFSET_HR       9       // KST = UTC+9

// WiFi credentials
#define WIFI_SSID                "your_wifi_ssid"
#define WIFI_PASSWORD            "your_wifi_password"

// ===================== PIN DEFINITIONS =====================
// ┌─────────────────────────────────────────────────────────────┐
// │  XIAO ESP32C6 Pin Assignment                                │
// ├──────────┬──────────┬───────────────────────────────────────┤
// │  Pin     │  GPIO    │  Function                             │
// ├──────────┼──────────┼───────────────────────────────────────┤
// │  D0      │  GPIO0   │  EPD BUSY                            │
// │  D1      │  GPIO1   │  EPD DC (Data/Command)               │
// │  D2      │  GPIO2   │  EPD RST (Reset)                     │
// │  D3      │  GPIO21  │  EPD CS (Chip Select)                │
// │  D4      │  GPIO22  │  I2C SDA (SHT40 + DS3231)            │
// │  D5      │  GPIO23  │  I2C SCL (SHT40 + DS3231)            │
// │  D6      │  GPIO16  │  (available)                         │
// │  D7      │  GPIO17  │  (available)                         │
// │  D8      │  GPIO19  │  SPI SCK                             │
// │  D9      │  GPIO20  │  SPI MISO (EPD 미사용, 예약)         │
// │  D10     │  GPIO18  │  SPI MOSI                            │
// └──────────┴──────────┴───────────────────────────────────────┘

// SPI Bus (e-paper only)
#define PIN_SPI_SCK     19   // D8
#define PIN_SPI_MISO    20   // D9  (EPD는 미사용, 예약)
#define PIN_SPI_MOSI    18   // D10

// E-Paper (2.9" BWR, SSD1680)
#define PIN_EPD_CS      21   // D3
#define PIN_EPD_DC       1   // D1
#define PIN_EPD_RST      2   // D2
#define PIN_EPD_BUSY     0   // D0

// I2C (SHT40 + DS3231)
#define PIN_I2C_SDA     22   // D4
#define PIN_I2C_SCL     23   // D5

// ===================== DISPLAY OBJECT =====================
// 2.9" BWR (3-color): GDEM029C90, 128x296, SSD1680
GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(
  GxEPD2_290_C90c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
);

// ===================== SENSOR & PERIPHERAL OBJECTS =====================
Adafruit_SHT4x  sht4;
DataLogger       logger;
DS3231_RTC       rtc;
DataWebServer    webServer(logger);

// ===================== RTC MEMORY (persists through deep sleep) =====================
RTC_DATA_ATTR uint32_t bootCount       = 0;
RTC_DATA_ATTR float    rtcMinTemp      = 999.0f;
RTC_DATA_ATTR float    rtcMaxTemp      = -999.0f;
RTC_DATA_ATTR float    rtcMinHum       = 999.0f;
RTC_DATA_ATTR float    rtcMaxHum       = -999.0f;
RTC_DATA_ATTR uint32_t lastWifiSyncBoot = 0;

// ===================== GLOBAL VARIABLES =====================
float    g_temperature = NAN;
float    g_humidity    = NAN;
bool     g_sht4_ok    = false;
bool     g_logger_ok  = false;
bool     g_rtc_ok     = false;
bool     g_wifi_ok    = false;
DateTime g_rtcNow;

// ===================== SETUP =====================
void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 2000) delay(10);

  bootCount++;
  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.printf ("║  Home Temp Monitor  -  Boot #%lu\n", bootCount);
  Serial.println("╚══════════════════════════════════════════╝");

  // ── Step 1: I2C Bus + Sensors ──
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  initRTC();
  initSHT40();
  readSensorData();
  updateMinMax();

  // ── Step 2: LittleFS (내장 플래시) ──
  initLogger();

  // ── Step 3: SPI + E-Paper ──
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  initEPD();
  updateDisplay();

  // ── Step 4: Log data ──
  logData();

  // ── Step 5: WiFi sync & Web server ──
  bool needWifiSync = (bootCount - lastWifiSyncBoot) >=
                       ((uint32_t)WIFI_SYNC_INTERVAL_HR * 60 / SLEEP_INTERVAL_MIN);

  if (needWifiSync || bootCount == 1) {
    startWifiAndSync();
  }

  // ── Step 6: Deep Sleep ──
  Serial.printf("\n>> Deep sleep for %d minutes...\n\n", SLEEP_INTERVAL_MIN);
  display.hibernate();
  esp_deep_sleep(SLEEP_INTERVAL_MIN * 60ULL * 1000000ULL);
}

void loop() {
  // Never reached — deep sleep resets the MCU
}

// ===================== RTC FUNCTIONS =====================
void initRTC() {
  if (!rtc.begin()) {
    Serial.println("[RTC] DS3231 not found!");
    g_rtc_ok = false;
    return;
  }
  g_rtc_ok = true;

  if (rtc.lostPower()) {
    Serial.println("[RTC] Power lost! Time needs sync via WiFi/NTP.");
  }

  g_rtcNow = rtc.now();
  Serial.printf("[RTC] %04d-%02d-%02d %02d:%02d:%02d\n",
    g_rtcNow.year, g_rtcNow.month, g_rtcNow.day,
    g_rtcNow.hour, g_rtcNow.minute, g_rtcNow.second);

  float rtcTemp = rtc.getTemperature();
  Serial.printf("[RTC] DS3231 temp: %.1f C\n", rtcTemp);
}

void syncRTCFromNTP() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[RTC] NTP time not available!");
    return;
  }

  DateTime dt;
  dt.year   = timeinfo.tm_year + 1900;
  dt.month  = timeinfo.tm_mon + 1;
  dt.day    = timeinfo.tm_mday;
  dt.hour   = timeinfo.tm_hour;
  dt.minute = timeinfo.tm_min;
  dt.second = timeinfo.tm_sec;
  dt.dow    = (timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday;

  rtc.adjust(dt);
  Serial.printf("[RTC] Synced from NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
    dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

// ===================== SHT40 FUNCTIONS =====================
void initSHT40() {
  if (!sht4.begin()) {
    if (!sht4.begin(0x45)) {
      Serial.println("[SHT40] Not found on 0x44 or 0x45!");
      g_sht4_ok = false;
      return;
    }
  }
  Serial.printf("[SHT40] Found. Serial: 0x%X\n", sht4.readSerial());
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);
  g_sht4_ok = true;
}

void readSensorData() {
  if (!g_sht4_ok) {
    g_temperature = NAN;
    g_humidity    = NAN;
    return;
  }

  sensors_event_t hum_event, temp_event;
  sht4.getEvent(&hum_event, &temp_event);

  g_temperature = temp_event.temperature;
  g_humidity    = hum_event.relative_humidity;

  Serial.printf("[SHT40] Temp: %.1f C | Hum: %.1f %%\n", g_temperature, g_humidity);
}

void updateMinMax() {
  if (!isnan(g_temperature)) {
    if (g_temperature < rtcMinTemp) rtcMinTemp = g_temperature;
    if (g_temperature > rtcMaxTemp) rtcMaxTemp = g_temperature;
  }
  if (!isnan(g_humidity)) {
    if (g_humidity < rtcMinHum) rtcMinHum = g_humidity;
    if (g_humidity > rtcMaxHum) rtcMaxHum = g_humidity;
  }
  Serial.printf("[MinMax] Temp: %.1f~%.1f C | Hum: %.1f~%.1f %%\n",
    rtcMinTemp, rtcMaxTemp, rtcMinHum, rtcMaxHum);
}

// ===================== DATA LOGGER FUNCTIONS =====================
void initLogger() {
  if (!logger.begin()) {
    Serial.println("[Logger] LittleFS init FAILED!");
    g_logger_ok = false;
    return;
  }
  g_logger_ok = true;
}

void logData() {
  if (!g_logger_ok) return;

  uint32_t ts = g_rtc_ok ? g_rtcNow.unixtime() : 0;
  float temp = isnan(g_temperature) ? -999.0f : g_temperature;
  float hum  = isnan(g_humidity)    ? -999.0f : g_humidity;

  if (logger.logData(ts, temp, hum)) {
    Serial.printf("[Logger] Saved: ts=%lu temp=%.1f hum=%.1f\n",
      ts, temp, hum);
  } else {
    Serial.println("[Logger] Write FAILED!");
  }
}

// ===================== WIFI & WEB SERVER =====================
void startWifiAndSync() {
  Serial.println("[WiFi] Starting WiFi for NTP sync & web server...");

  webServer.onTimeSync([]() {
    syncRTCFromNTP();
  });

  webServer.start(WIFI_SSID, WIFI_PASSWORD);

  if (webServer.isRunning()) {
    g_wifi_ok = true;
    lastWifiSyncBoot = bootCount;

    Serial.printf("[WebServer] Active for %d seconds. Connect to download CSV.\n",
      WIFI_AP_TIMEOUT_SEC);

    unsigned long startMs = millis();
    while (millis() - startMs < (unsigned long)WIFI_AP_TIMEOUT_SEC * 1000) {
      webServer.handleClient();
      delay(10);
    }

    webServer.stop();
    Serial.println("[WebServer] Timeout. WiFi disconnected.");
  } else {
    g_wifi_ok = false;
    Serial.println("[WiFi] Connection failed. Will retry next cycle.");
  }
}

// ===================== E-PAPER DISPLAY =====================
void initEPD() {
  display.init(SERIAL_BAUD, true, 50, false);
  display.setRotation(0); // Portrait: 128 x 296
  Serial.println("[EPD] 2.9\" BWR initialized (portrait 128x296).");
}

void updateDisplay() {
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawLayout();
  } while (display.nextPage());

  Serial.println("[EPD] Display updated.");
}

void drawLayout() {
  // ─── Display: 128 x 296 (portrait) ───
  //
  //  ┌──────────────────────┐ y=0
  //  │  Home Climate        │ 12pt  y=18
  //  │══════════════════════│ y=24
  //  │                      │
  //  │  TEMPERATURE (빨강)  │ 9pt   y=44
  //  │                      │
  //  │    24.5°C            │ 24pt  y=82
  //  │                      │
  //  │  22.1~26.3°C (빨강) │ 9pt   y=100
  //  │                      │
  //  │──────────────────────│ y=112
  //  │                      │
  //  │  HUMIDITY            │ 9pt   y=130
  //  │                      │
  //  │    45.2%             │ 24pt  y=168
  //  │                      │
  //  │  40.0~55.2%          │ 9pt   y=186
  //  │                      │
  //  │══════════════════════│ y=200
  //  │  2026-06-30 07:23    │ 9pt   y=216
  //  │  SHT:OK  Log:OK      │ 9pt   y=232
  //  │  #42  5min  ~154d    │ 9pt   y=248
  //  └──────────────────────┘

  // ── Title ──
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(4, 18);
  display.print("Home Climate");

  drawHLine(0, 24, 128, 2);

  // ── Temperature Section ──
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_RED);
  display.setCursor(4, 44);
  display.print("TEMPERATURE");

  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(8, 82);
  if (!isnan(g_temperature)) {
    display.printf("%.1f", g_temperature);
  } else {
    display.print("--.-");
  }

  display.setFont(&FreeSansBold12pt7b);
  int16_t tx, ty; uint16_t tw, th;
  display.getTextBounds("24.5", 8, 82, &tx, &ty, &tw, &th);
  display.setCursor(8 + tw + 2, 72);
  display.print("\xB0" "C");

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_RED);
  display.setCursor(4, 100);
  if (rtcMinTemp < 900.0f && rtcMaxTemp > -900.0f) {
    display.printf("%.1f~%.1f\xB0" "C", rtcMinTemp, rtcMaxTemp);
  } else {
    display.print("---");
  }

  drawHLine(0, 112, 128, 1);

  // ── Humidity Section ──
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(4, 130);
  display.print("HUMIDITY");

  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(8, 168);
  if (!isnan(g_humidity)) {
    display.printf("%.1f%%", g_humidity);
  } else {
    display.print("--.-%");
  }

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(4, 186);
  if (rtcMinHum < 900.0f && rtcMaxHum > -900.0f) {
    display.printf("%.1f~%.1f%%", rtcMinHum, rtcMaxHum);
  } else {
    display.print("---");
  }

  // ── Bottom Info Section ──
  drawHLine(0, 200, 128, 2);

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  display.setCursor(4, 216);
  if (g_rtc_ok) {
    display.printf("%02d-%02d %02d:%02d",
      g_rtcNow.month, g_rtcNow.day, g_rtcNow.hour, g_rtcNow.minute);
  } else {
    display.print("RTC:FAIL");
  }

  display.setCursor(4, 232);
  display.printf("SHT:%s Log:%s",
    g_sht4_ok ? "OK" : "X",
    g_logger_ok ? "OK" : "X");

  display.setCursor(4, 248);
  display.printf("#%lu %dmin ~%dd",
    bootCount, SLEEP_INTERVAL_MIN, estimateBatteryDays());
}

// ===================== UTILITY FUNCTIONS =====================
int estimateBatteryDays() {
  const float I_SLEEP_MA   = 0.015f;
  const float I_ACTIVE_MA  = 80.0f;
  const float T_ACTIVE_S   = 10.0f;
  const float T_CYCLE_S    = SLEEP_INTERVAL_MIN * 60.0f;
  const float T_SLEEP_S    = T_CYCLE_S - T_ACTIVE_S;
  const float BATTERY_MAH  = 10000.0f;

  float wifiExtraPerCycle = (120.0f * 120.0f) / (WIFI_SYNC_INTERVAL_HR * 3600.0f / T_CYCLE_S);

  float avgCurrentMA = (I_SLEEP_MA * T_SLEEP_S + I_ACTIVE_MA * T_ACTIVE_S + wifiExtraPerCycle) / T_CYCLE_S;
  float lifeHours    = BATTERY_MAH / avgCurrentMA;
  return (int)(lifeHours / 24.0f);
}

void drawHLine(int x, int y, int w, int thickness) {
  for (int i = 0; i < thickness; i++) {
    display.drawLine(x, y + i, x + w - 1, y + i, GxEPD_BLACK);
  }
}

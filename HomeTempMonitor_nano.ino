/*
 * ═══════════════════════════════════════════════════════════════
 *  Home Temperature & Humidity Monitor
 *  nanoESP32-C6 + SHT40 + 2.9" E-Paper + DS3231
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
 *  ✅ 버튼(GPIO8) → 언제든 WiFi 켜서 CSV 다운로드
 *  ✅ WiFi AP 모드 → 공유기 없이도 직접 접속 가능
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
 *  - Board: "ESP32C6 Dev Module"
 *
 * 파티션 설정:
 *  - Tools → Partition Scheme →
 *    "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
 *
 * ⚠️ 주의: nanoESP32-C6는 배터리 충전 회로가 없습니다.
 *    Li-Po 배터리 사용 시 외부 충전 모듈(TP4056 등)이 필요합니다.
 */

// ===================== INCLUDES =====================
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_NeoPixel.h>
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
#define WIFI_AP_TIMEOUT_SEC      300     // Web server active duration (seconds)
#define WIFI_AP_SSID              "HomeTemp"  // AP 모드 이름 (비번 없음)
#define SERIAL_BAUD              115200
#define TIMEZONE_OFFSET_HR       9       // KST = UTC+9

// WiFi credentials
#define WIFI_SSID                "your_wifi_ssid"
#define WIFI_PASSWORD            "your_wifi_password"

// ===================== PIN DEFINITIONS =====================
// ┌─────────────────────────────────────────────────────────────┐
// │  nanoESP32-C6 Pin Assignment                                │
// ├──────────┬──────────┬───────────────────────────────────────┤
// │  GPIO    │  핀 이름 │  Function                             │
// ├──────────┼──────────┼───────────────────────────────────────┤
// │  GPIO0   │  D0      │  EPD BUSY                            │
// │  GPIO1   │  D1      │  EPD DC (Data/Command)               │
// │  GPIO2   │  D2      │  SPI SCK                             │
// │  GPIO3   │  D3      │  SPI MISO (EPD 미사용, 예약)         │
// │  GPIO4   │  D4      │  SPI MOSI                            │
// │  GPIO5   │  D5      │  EPD CS (Chip Select)                │
// │  GPIO6   │  D6      │  I2C SDA (SHT40 + DS3231)            │
// │  GPIO7   │  D7      │  I2C SCL (SHT40 + DS3231)            │
// │  GPIO8   │  D8      │  User Button (WiFi 트리거)           │
// │  GPIO9   │  D9      │  EPD RST (Reset)                     │
// │  GPIO10  │  D10     │  (available)                         │
// │  GPIO11  │  D11     │  (available)                         │
// │  GPIO12  │  D12     │  UART0 TX (디버그 직렬)               │
// │  GPIO13  │  D13     │  UART0 RX (디버그 직렬)               │
// │  GPIO14  │  D14     │  (available)                         │
// │  GPIO15  │  D15     │  EPD DC (대체)                        │
// │  GPIO16  │  D16     │  (available)                         │
// │  GPIO17  │  D17     │  (available)                         │
// │  GPIO18  │  D18     │  (available)                         │
// │  GPIO19  │  D19     │  (available)                         │
// │  GPIO20  │  D20     │  (available)                         │
// │  GPIO21  │  D21     │  (available)                         │
// │  GPIO22  │  D22     │  (available)                         │
// │  GPIO23  │  D23     │  (available)                         │
// │  GPIO8   │  RGB LED │  WS2812 (사용 안 함)                  │
// └──────────┴──────────┴───────────────────────────────────────┘
//
// ⚠️ nanoESP32-C6 주의사항:
//   - GPIO8은 RGB LED(WS2812)와 공유 → 버튼 사용 시 LED 비활성화 필요
//   - GPIO12/13은 CH343 USB-UART와 공유 → 디버그 직렬로 사용 중
//   - 배터리 충전 회로 없음 → 외부 TP4056 등 필요

// SPI Bus (e-paper)
#define PIN_SPI_SCK      2   // D2  (GPIO2)
#define PIN_SPI_MISO     3   // D3  (GPIO3) — EPD 미사용, 예약
#define PIN_SPI_MOSI     4   // D4  (GPIO4)

// E-Paper (2.9" BWR, SSD1680)
#define PIN_EPD_CS       5   // D5  (GPIO5)
#define PIN_EPD_DC       1   // D1  (GPIO1)
#define PIN_EPD_RST      9   // D9  (GPIO9)
#define PIN_EPD_BUSY     0   // D0  (GPIO0)

// I2C (SHT40 + DS3231) — ESP32-C6-MINI-1 기본 I2C 핀
#define PIN_I2C_SDA      6   // D6  (GPIO6)
#define PIN_I2C_SCL      7   // D7  (GPIO7)

// User Button (WiFi 트리거)
// ⚠️ GPIO8은 RGB LED와 공유 → 버튼 사용 시 LED 비활성화 필요
#define PIN_BUTTON       8   // D8  (GPIO8) — LOW = pressed

// RGB LED (WS2812) — GPIO8과 공유, 버튼 사용 시 반드시 끄기
#define PIN_RGB_LED      8   // GPIO8 (WS2812)
#define NUM_RGB_LEDS     1
Adafruit_NeoPixel rgbLED(NUM_RGB_LEDS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);

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
RTC_DATA_ATTR bool     forceWebServer  = false;  // 버튼 눌림 플래그

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
  // ── RGB LED 끄기 (GPIO8 공유 → 버튼 사용 위해 LED 비활성화) ──
  // WS2812는 전원 인가 시 랜덤 색상이 표시될 수 있으므로
  // 가장 먼저 꺼야 함 (전력 절약 + GPIO8 버튼 충돌 방지)
  rgbLED.begin();
  rgbLED.setPixelColor(0, rgbLED.Color(0, 0, 0));  // 검정 = OFF
  rgbLED.show();
  pinMode(PIN_RGB_LED, INPUT);  // LED 핀을 입력으로 전환 → 버튼과 공유 핀 충돌 방지
  Serial.println("[RGB] LED turned OFF (GPIO8 shared with button)");

  // ── 버튼 핀 최우선 설정 (floating 방지) ──
  // Deep Sleep 웨이크업 직후에도 안정적인 HIGH 보장
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(1);  // 풀업 안정화 대기

  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 2000) delay(10);

  bootCount++;

  // ── 웨이크업 원인 확인 ──
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
  const char* wakeupStr = "UNKNOWN";
  if (wakeupReason == ESP_SLEEP_WAKEUP_TIMER)  wakeupStr = "TIMER";
  else if (wakeupReason == ESP_SLEEP_WAKEUP_EXT1) wakeupStr = "BUTTON (GPIO8)";
  else if (wakeupReason == ESP_SLEEP_WAKEUP_UNDEFINED) wakeupStr = "POWER_ON/RESET";

  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.printf ("║  Home Temp Monitor  -  Boot #%lu\n", bootCount);
  Serial.printf ("║  Board: nanoESP32-C6\n");
  Serial.printf ("║  Wakeup: %s\n", wakeupStr);
  Serial.println("╚══════════════════════════════════════════╝");

  // ── Step 1: I2C Bus + Sensors ──
  Serial.println("[SETUP] Step 1: I2C + Sensors...");
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("[I2C]  SDA=GPIO%d, SCL=GPIO%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
  initRTC();
  initSHT40();
  readSensorData();
  updateMinMax();

  // ── Step 2: LittleFS (내장 플래시) ──
  Serial.println("[SETUP] Step 2: LittleFS...");
  initLogger();

  // ── Step 3: SPI + E-Paper ──
  Serial.println("[SETUP] Step 3: SPI + E-Paper...");
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  Serial.printf("[SPI]  SCK=GPIO%d, MISO=GPIO%d, MOSI=GPIO%d\n",
    PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  initEPD();
  updateDisplay();

  // ── Step 4: Log data ──
  Serial.println("[SETUP] Step 4: Log data...");
  logData();

  // ── Step 5: 버튼 체크 (GPIO8 눌림 시 WiFi 즉시 활성화) ──
  // 주의: 이미 setup() 최상단에서 pinMode(INPUT_PULLUP) 설정됨
  bool buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
  // 웨이크업 원인이 버튼이면 버튼을 뗐어도 WiFi 켜기
  bool wokeByButton = (wakeupReason == ESP_SLEEP_WAKEUP_EXT1);
  Serial.printf("[SETUP] Step 5: Button GPIO8=%s, wokeByButton=%s\n",
    buttonPressed ? "PRESSED" : "released", wokeByButton ? "YES" : "no");

  // ── Step 6: WiFi sync & Web server ──
  bool needWifiSync = (bootCount - lastWifiSyncBoot) >=
                       ((uint32_t)WIFI_SYNC_INTERVAL_HR * 60 / SLEEP_INTERVAL_MIN);

  Serial.printf("[SETUP] Step 6: WiFi check — forceWebServer=%d, button=%d, needSync=%d, bootCount=%lu\n",
    (int)forceWebServer, (int)buttonPressed, (int)needWifiSync, bootCount);

  if (forceWebServer || buttonPressed || wokeByButton || needWifiSync || bootCount == 1) {
    forceWebServer = false;  // 1회만
    Serial.println("[SETUP] → Starting WiFi & Web server...");
    startWifiAndSync();
  } else {
    Serial.println("[SETUP] → WiFi skipped (not needed this cycle).");
  }

  // ── Step 7: Deep Sleep ──
  Serial.println("[SETUP] Step 7: Deep sleep DISABLED for debugging");
  // ESP32C6는 ext0 미지원 → ext1 사용 (GPIO 비트마스크)
  // GPIO8 LOW(버튼 누름) 시 웨이크업
  //esp_sleep_enable_ext1_wakeup(BIT(GPIO_NUM_8), ESP_EXT1_WAKEUP_ALL_LOW);
  //esp_sleep_enable_timer_wakeup(SLEEP_INTERVAL_MIN * 60ULL * 1000000ULL);

  int battDays = estimateBatteryDays();
  Serial.printf(">> Deep sleep DISABLED (debug mode)\n");
  Serial.printf(">> Battery estimate: ~%d days remaining\n", battDays);
  Serial.printf(">> Next WiFi sync in %lu boots\n",
    (uint32_t)WIFI_SYNC_INTERVAL_HR * 60 / SLEEP_INTERVAL_MIN - (bootCount - lastWifiSyncBoot));

  //display.hibernate();
  //esp_deep_sleep_start();
  Serial.println(">> Setup complete. Entering loop() for debug...");
}

void loop() {
  // Deep sleep 비활성화 시 디버그용 루프
  static unsigned long lastReadMs = 0;
  if (millis() - lastReadMs >= 10000) {  // 10초마다 재측정
    lastReadMs = millis();
    Serial.println("\n--- loop() periodic read ---");
    readSensorData();
    updateMinMax();
    logData();
    updateDisplay();
    Serial.printf("[LOOP] Temp: %.1f C | Hum: %.1f %% | Boot: %lu\n",
      g_temperature, g_humidity, bootCount);
  }
  delay(100);
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
  // SHT40 기본 주소: 0x44. 대체 주소: 0x45
  // Adafruit_SHT4x 라이브러리는 begin(TwoWire*)만 지원
  if (!sht4.begin(&Wire)) {
    Serial.println("[SHT40] Not found at 0x44! Trying 0x45...");
    Wire.beginTransmission(0x45);
    if (Wire.endTransmission() == 0) {
      Serial.println("[SHT40] Device at 0x45 but library only supports 0x44.");
    }
    Serial.println("[SHT40] Sensor not found!");
    g_sht4_ok = false;
    return;
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
  // ── 먼저 공유기(Station) 모드 시도 ──
  Serial.println("[WiFi] Trying Station mode (공유기 연결)...");

  webServer.onTimeSync([]() {
    syncRTCFromNTP();
  });

  webServer.start(WIFI_SSID, WIFI_PASSWORD);

  if (webServer.isRunning()) {
    // 공유기 연결 성공 → NTP 동기화 + 웹서버
    g_wifi_ok = true;
    lastWifiSyncBoot = bootCount;
    runWebServer();
  } else {
    // 공유기 연결 실패 → AP 모드로 전환 (직접 접속)
    Serial.println("[WiFi] Station failed. Starting AP mode...");
    startAPMode();
  }
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID);  // 개방형 AP (비번 없음)

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi AP] SSID: \"%s\"\n", WIFI_AP_SSID);
  Serial.printf("[WiFi AP] Connect to WiFi \"%s\" then open http://%s\n",
    WIFI_AP_SSID, apIP.toString().c_str());

  // E-Paper에 AP 정보 표시
  showAPInfo(apIP);

  // 간이 웹서버 시작
  WiFiServer apServer(80);
  apServer.begin();

  unsigned long startMs = millis();
  while (millis() - startMs < (unsigned long)WIFI_AP_TIMEOUT_SEC * 1000) {
    WiFiClient client = apServer.available();
    if (client) {
      handleAPRequest(client);
    }
    delay(10);
  }

  apServer.stop();
  WiFi.softAPdisconnect(true);
  Serial.println("[WiFi AP] Timeout. AP stopped.");
}

void handleAPRequest(WiFiClient &client) {
  String request = "";
  unsigned long timeout = millis() + 3000;
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (request.endsWith("\r\n\r\n")) break;
    }
  }

  if (request.indexOf("GET /data.csv") >= 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/csv; charset=utf-8");
    client.println("Content-Disposition: attachment; filename=\"temp_data.csv\"");
    client.println("Connection: close");
    client.println();
    logger.readAllCSV(client);
  } else {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=utf-8");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    client.println("<title>Home Temp Monitor</title></head><body>");
    client.println("<h1>Home Temp Monitor (AP Mode)</h1>");
    client.printf("<p>Records: %lu | File: %u bytes</p>",
      logger.getRecordCount(), logger.getFileSize());
    client.println("<br><a href='/data.csv' style='padding:15px 30px;background:#4CAF50;"
      "color:#fff;text-decoration:none;border-radius:8px;font-size:18px'>"
      "Download CSV</a>");
    client.println("</body></html>");
  }

  delay(10);
  client.stop();
}

void runWebServer() {
  Serial.printf("[WebServer] Active for %d seconds. Connect to download CSV.\n",
    WIFI_AP_TIMEOUT_SEC);

  // E-Paper에 IP 표시
  showIPInfo(WiFi.localIP());

  unsigned long startMs = millis();
  while (millis() - startMs < (unsigned long)WIFI_AP_TIMEOUT_SEC * 1000) {
    webServer.handleClient();
    delay(10);
  }

  webServer.stop();
  Serial.println("[WebServer] Timeout. WiFi disconnected.");
}

void showIPInfo(IPAddress ip) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(4, 30);
    display.print("WiFi Connected!");
    drawHLine(0, 36, 128, 2);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(4, 56);
    display.print("Open browser:");
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(4, 78);
    display.printf("http://%s", ip.toString().c_str());
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(4, 100);
    display.printf("Timeout: %d sec", WIFI_AP_TIMEOUT_SEC);
    display.setCursor(4, 120);
    display.print("Download CSV now!");
  } while (display.nextPage());
}

void showAPInfo(IPAddress ip) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_RED);
    display.setCursor(4, 30);
    display.print("WiFi AP Mode");
    drawHLine(0, 36, 128, 2);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(4, 56);
    display.print("1. Connect WiFi:");
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(4, 78);
    display.print(WIFI_AP_SSID);
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(4, 98);
    display.print("2. Open browser:");
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(4, 120);
    display.printf("http://%s", ip.toString().c_str());
  } while (display.nextPage());
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

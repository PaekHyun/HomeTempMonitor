/*
 * ═══════════════════════════════════════════════════════════════
 *  Home Temperature & Humidity Monitor
 *  ESP32-C6 + SHT40 + 2.9" 4-Color E-Paper + DS3231
 *  (내장 플래시 LittleFS 저장)
 * ═══════════════════════════════════════════════════════════════
 *
 * Features:
 *  ✅ SHT40 sensor → temperature & humidity (I2C)
 *  ✅ 2.9" BWRY e-paper → 4-color display (SPI, JD79667, 168×384)
 *  ✅ 내장 플래시 LittleFS → CSV 데이터 저장
 *  ✅ DS3231 RTC → 정확한 타임스탬프 (I2C)
 *  ✅ WiFi NTP → 24시간마다 시간 동기화
 *  ✅ Web server → 브라우저에서 CSV 다운로드
 *  ✅ 버튼 → 언제든 WiFi 켜서 CSV 다운로드
 *  ✅ WiFi AP 모드 → 공유기 없이도 직접 접속 가능
 *  ✅ Deep sleep → 초저전력 (~15uA)
 *  ✅ Min/Max 추적 (RTC 메모리 유지)
 *  ✅ 4색 활용 → Black/White/Red/Yellow
 *
 * Board Selection:
 *  아래 BOARD_SELECT 중 하나만 #define 하세요.
 *    BOARD_XIAO_ESP32C6  → Seeed XIAO ESP32C6 (4MB Flash)
 *    BOARD_NANO_ESP32C6  → nanoESP32-C6 (16MB Flash)
 *
 * Required Libraries (Arduino Library Manager):
 *  - GxEPD2 by Jean-Marc Capello
 *  - Adafruit SHT4x Library
 *  - Adafruit GFX Library (GxEPD2 dependency)
 *  - Adafruit NeoPixel (nanoESP32-C6만 필요, XIAO는 미사용)
 *
 * Board Package:
 *  - esp32 by Espressif v3.0.0+ (ESP32C6 support)
 *  - XIAO: Board "XIAO_ESP32C6"
 *  - nano: Board "ESP32C6 Dev Module"
 *
 * 파티션 설정:
 *  - XIAO (4MB): "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
 *  - nano (16MB): Custom → partitions_16mb.csv
 */

// ===================== BOARD SELECTION =====================
// ✏️ 사용할 보드를 하나만 #define 하세요. 나머지는 주석 처리!
// #define BOARD_XIAO_ESP32C6
#define BOARD_NANO_ESP32C6

#if !defined(BOARD_XIAO_ESP32C6) && !defined(BOARD_NANO_ESP32C6)
  #error "BOARD_XIAO_ESP32C6 또는 BOARD_NANO_ESP32C6 중 하나를 #define 하세요!"
#endif
#if defined(BOARD_XIAO_ESP32C6) && defined(BOARD_NANO_ESP32C6)
  #error "보드는 하나만 선택할 수 있습니다! 하나를 주석 처리하세요."
#endif

// ===================== INCLUDES =====================
#include <GxEPD2_4C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Adafruit_SHT4x.h>
#ifdef BOARD_NANO_ESP32C6
  #include <Adafruit_NeoPixel.h>
#endif
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
#define WIFI_SSID                "darmi"
#define WIFI_PASSWORD            "rkdmf800412"

// ===================== PIN DEFINITIONS =====================
// ┌──────────┬──────────────────┬──────────────────┬───────────────────┐
// │  기능     │  XIAO ESP32C6    │  nanoESP32-C6    │  비고             │
// ├──────────┼──────────────────┼──────────────────┼───────────────────┤
// │  SPI SCK │  GPIO19 (D8)     │  GPIO5  (D5)     │  EPD CLK          │
// │  SPI MISO│  GPIO20 (D9)     │  (미사용)         │  EPD 미사용        │
// │  SPI MOSI│  GPIO18 (D10)    │  GPIO4  (D4)     │  EPD DIN          │
// │  EPD CS  │  GPIO21 (D3)     │  GPIO6  (D6)     │                   │
// │  EPD DC  │  GPIO1  (D1)     │  GPIO7  (D7)     │                   │
// │  EPD RST │  GPIO2  (D2)     │  GPIO0  (D0)     │                   │
// │  EPD BUSY│  GPIO0  (D0)     │  GPIO1  (D1)     │                   │
// │  I2C SDA │  GPIO22 (D4)     │  GPIO22          │  SHT40 + DS3231   │
// │  I2C SCL │  GPIO23 (D5)     │  GPIO23          │  SHT40 + DS3231   │
// │  Button  │  GPIO16 (D6)     │  GPIO8  (D8)     │  LOW = pressed    │
// └──────────┴──────────────────┴──────────────────┴───────────────────┘

#ifdef BOARD_XIAO_ESP32C6
  #define PIN_SPI_SCK     19
  #define PIN_SPI_MISO    20
  #define PIN_SPI_MOSI    18
  #define PIN_EPD_CS      21
  #define PIN_EPD_DC       1
  #define PIN_EPD_RST      2
  #define PIN_EPD_BUSY     0
  #define PIN_I2C_SDA     22
  #define PIN_I2C_SCL     23
  #define PIN_BUTTON      16
  #define BOARD_NAME      "XIAO ESP32C6"
  #define BUTTON_WAKE_GPIO GPIO_NUM_16
#elif defined(BOARD_NANO_ESP32C6)
  #define PIN_SPI_SCK      5
  #define PIN_SPI_MISO    -1
  #define PIN_SPI_MOSI     4
  #define PIN_EPD_CS       6
  #define PIN_EPD_DC       7
  #define PIN_EPD_RST      0
  #define PIN_EPD_BUSY     1
  #define PIN_I2C_SDA     22
  #define PIN_I2C_SCL     23
  #define PIN_BUTTON       8
  #define PIN_RGB_LED      8
  #define NUM_RGB_LEDS     1
  #define BOARD_NAME      "nanoESP32-C6"
  #define BUTTON_WAKE_GPIO GPIO_NUM_8
#endif

// ===================== BOARD-SPECIFIC OBJECTS =====================
#ifdef BOARD_NANO_ESP32C6
  Adafruit_NeoPixel rgbLED(NUM_RGB_LEDS, PIN_RGB_LED, NEO_GRB + NEO_KHZ800);
#endif

// ===================== DISPLAY OBJECT =====================
// GDEY029F51H: 2.9" 4-color (BWRY), 168×384, JD79667
GxEPD2_4C<GxEPD2_290c_GDEY029F51H, GxEPD2_290c_GDEY029F51H::HEIGHT> display(
  GxEPD2_290c_GDEY029F51H(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
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
RTC_DATA_ATTR bool     forceWebServer  = false;

// ===================== GLOBAL VARIABLES =====================
float    g_temperature = NAN;
float    g_humidity    = NAN;
bool     g_sht4_ok    = false;
bool     g_logger_ok  = false;
bool     g_rtc_ok     = false;
bool     g_wifi_ok    = false;
DateTime g_rtcNow;

// ===================== FORWARD DECLARATIONS =====================
void initRTC();
void syncRTCFromNTP();
void initSHT40();
void readSensorData();
void updateMinMax();
void initLogger();
void logData();
void initEPD();
void updateDisplay();
void drawLayout();
void startWifiAndSync();
void startAPMode();
void handleAPRequest(WiFiClient &client);
void runWebServer();
void showIPInfo(IPAddress ip);
void showAPInfo(IPAddress ip);
int  estimateBatteryDays();
void drawHLine(int x, int y, int w, int thickness);
float calcHeatIndex(float tempC, float humidity);

// ===================== SETUP =====================
void setup() {
#ifdef BOARD_NANO_ESP32C6
  rgbLED.begin();
  rgbLED.setPixelColor(0, rgbLED.Color(0, 0, 0));
  rgbLED.show();
  pinMode(PIN_RGB_LED, INPUT);
  Serial.println("[RGB] LED turned OFF (GPIO8 shared with button)");
#endif

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(1);

  Serial.begin(SERIAL_BAUD);
  while (!Serial && millis() < 2000) delay(10);

  bootCount++;

  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
  const char* wakeupStr = "UNKNOWN";
  if (wakeupReason == ESP_SLEEP_WAKEUP_TIMER)  wakeupStr = "TIMER";
  else if (wakeupReason == ESP_SLEEP_WAKEUP_EXT1) wakeupStr = "BUTTON (" BOARD_NAME ")";
  else if (wakeupReason == ESP_SLEEP_WAKEUP_UNDEFINED) wakeupStr = "POWER_ON/RESET";

  Serial.println();
  Serial.println("╔══════════════════════════════════════════╗");
  Serial.printf ("║  Home Temp Monitor  -  Boot #%lu\n", bootCount);
  Serial.printf ("║  Board: %s\n", BOARD_NAME);
  Serial.printf ("║  Wakeup: %s\n", wakeupStr);
  Serial.println("╚══════════════════════════════════════════╝");

  // Step 1: I2C + Sensors
  Serial.println("[SETUP] Step 1: I2C + Sensors...");
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial.printf("[I2C]  SDA=GPIO%d, SCL=GPIO%d\n", PIN_I2C_SDA, PIN_I2C_SCL);
  initRTC();
  initSHT40();
  readSensorData();
  updateMinMax();

  // Step 2: LittleFS
  Serial.println("[SETUP] Step 2: LittleFS...");
  initLogger();

  // Step 3: SPI + E-Paper
  Serial.println("[SETUP] Step 3: SPI + E-Paper...");
  SPI.begin(PIN_SPI_SCK, -1, PIN_SPI_MOSI, PIN_EPD_CS);
  Serial.printf("[SPI]  SCK=GPIO%d, MOSI=GPIO%d, CS=GPIO%d\n",
    PIN_SPI_SCK, PIN_SPI_MOSI, PIN_EPD_CS);
  initEPD();
  updateDisplay();

  // Step 4: Log data
  Serial.println("[SETUP] Step 4: Log data...");
  logData();

  // Step 5: 버튼 체크
  bool buttonPressed = (digitalRead(PIN_BUTTON) == LOW);
  bool wokeByButton = (wakeupReason == ESP_SLEEP_WAKEUP_EXT1);
  Serial.printf("[SETUP] Step 5: Button GPIO%d=%s, wokeByButton=%s\n",
    PIN_BUTTON, buttonPressed ? "PRESSED" : "released", wokeByButton ? "YES" : "no");

  // Step 6: WiFi sync & Web server
  bool needWifiSync = (bootCount - lastWifiSyncBoot) >=
                       ((uint32_t)WIFI_SYNC_INTERVAL_HR * 60 / SLEEP_INTERVAL_MIN);

  Serial.printf("[SETUP] Step 6: WiFi check — forceWebServer=%d, button=%d, needSync=%d, bootCount=%lu\n",
    (int)forceWebServer, (int)buttonPressed, (int)needWifiSync, bootCount);

  if (forceWebServer || buttonPressed || wokeByButton || needWifiSync || bootCount == 1) {
    forceWebServer = false;
    Serial.println("[SETUP] → Starting WiFi & Web server...");
    startWifiAndSync();
    Serial.println("[SETUP] → WiFi work done, restoring temp/humidity display...");
    updateDisplay();
  } else {
    Serial.println("[SETUP] → WiFi skipped (not needed this cycle).");
  }

  // Step 7: Deep Sleep (현재 비활성화, 디버그 모드)
  Serial.println("[SETUP] Step 7: Deep sleep DISABLED for debugging");
  //esp_sleep_enable_ext1_wakeup(BIT(BUTTON_WAKE_GPIO), ESP_EXT1_WAKEUP_ALL_LOW);
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
  static unsigned long lastReadMs = 0;
  if (millis() - lastReadMs >= 300000) {  // 5분마다 재측정
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
  Serial.println("[WiFi] Trying Station mode (공유기 연결)...");

  webServer.onTimeSync([]() {
    syncRTCFromNTP();
  });

  webServer.start(WIFI_SSID, WIFI_PASSWORD);

  if (webServer.isRunning()) {
    g_wifi_ok = true;
    lastWifiSyncBoot = bootCount;
    runWebServer();
  } else {
    Serial.println("[WiFi] Station failed. Starting AP mode...");
    startAPMode();
  }
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[WiFi AP] SSID: \"%s\"\n", WIFI_AP_SSID);
  Serial.printf("[WiFi AP] Connect to WiFi \"%s\" then open http://%s\n",
    WIFI_AP_SSID, apIP.toString().c_str());

  showAPInfo(apIP);

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

  showIPInfo(WiFi.localIP());

  unsigned long startMs = millis();
  while (millis() - startMs < (unsigned long)WIFI_AP_TIMEOUT_SEC * 1000) {
    webServer.handleClient();
    delay(10);
  }

  webServer.stop();
  Serial.println("[WebServer] Timeout. WiFi disconnected.");
}

// ===================== E-PAPER INFO SCREENS =====================
void showIPInfo(IPAddress ip) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeSansBold18pt7b);    
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(6, 40);
    display.print("WiFi");

    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(6, 70);
    display.print("Connected!");

    drawHLine(0, 82, 168, 3);

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(6, 112);
    display.print("Open browser:");

    // display.setFont(&FreeSansBold18pt7b);
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(6, 148);
    display.printf("http://");
    display.setCursor(6, 180);
    display.print(ip.toString().c_str());

    drawHLine(0, 196, 168, 2);

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(6, 224);
    display.printf("Timeout: %d sec", WIFI_AP_TIMEOUT_SEC);

    display.setCursor(6, 254);
    display.print("Download CSV now!");
  } while (display.nextPage());
}

void showAPInfo(IPAddress ip) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    display.setFont(&FreeSansBold18pt7b);
    display.setTextColor(GxEPD_RED);
    display.setCursor(6, 40);
    display.print("WiFi AP");
    display.setCursor(6, 70);
    display.print("Mode");

    drawHLine(0, 82, 168, 3);

    display.setFont(&FreeSansBold12pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(6, 112);
    display.print("1. Connect WiFi:");

    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(6, 148);
    display.print(WIFI_AP_SSID);

    drawHLine(0, 164, 168, 1);

    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(6, 196);
    display.print("2. Open browser:");

    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(6, 232);
    display.printf("http://");
    display.setCursor(6, 264);
    display.print(ip.toString().c_str());
  } while (display.nextPage());
}

// ===================== E-PAPER DISPLAY =====================
void initEPD() {
  display.init(115200, true, 5, false);
  display.setRotation(0); // Portrait: 168 × 384
  Serial.println("[EPD] 2.9\" 4-color (BWRY) initialized (portrait 168x384).");
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

// ── 체감온도 계산 (Heat Index, Steadman 공식) ──
float calcHeatIndex(float tempC, float humidity) {
  if (isnan(tempC) || isnan(humidity)) return NAN;
  if (tempC < 27.0f) return tempC;  // 27°C 미만은 보정 불필요
  float T = tempC;
  float R = humidity;
  float HI = 0.5f * (T + 61.0f + ((T - 68.0f) * 1.2f) + (R * 0.094f));
  // Rothfusz 회귀 (HI >= 80°F ≈ 26.7°C)
  if (HI > 26.7f) {
    float F = T * 9.0f / 5.0f + 32.0f;
    HI = -42.379f + 2.04901523f*F + 10.14333127f*R
         - 0.22475541f*F*R - 0.00683783f*F*F
         - 0.05481717f*R*R + 0.00122874f*F*F*R
         + 0.00085282f*F*R*R - 0.00000199f*F*F*R*R;
    HI = (HI - 32.0f) * 5.0f / 9.0f;  // °F → °C
  }
  return HI;
}

// ── 메인 레이아웃 (168 × 384 전체 활용, 4색 BWRY) ──
void drawLayout() {
  const int W = 168;

  // ══════════════════════════════════════════
  //  Title Section (y: 0 ~ 38)
  // ══════════════════════════════════════════
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(6, 28);
  display.print("Home Climate");

  drawHLine(0, 36, W, 3);

  // ══════════════════════════════════════════
  //  Temperature Section (y: 36 ~ 170)
  // ══════════════════════════════════════════
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_RED);
  display.setCursor(6, 58);
  display.print("TEMPERATURE");

  display.setFont(&FreeSansBold24pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(10, 102);
  if (!isnan(g_temperature)) {
    display.printf("%.1f", g_temperature);
  } else {
    display.print("--.-");
  }

  // °C 표시
  // display.setFont(&FreeSansBold12pt7b);
  display.setFont(&FreeSansBold24pt7b);
  int16_t tx, ty; uint16_t tw, th;
  display.getTextBounds("24.5", 10, 102, &tx, &ty, &tw, &th);
  // display.setCursor(10 + tw + 4, 90);
  display.setCursor(10 + tw + 4, 102);
  display.print("\xB0" "C");

  // Min/Max (빨강)
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_RED);
  display.setCursor(6, 124);
  if (rtcMinTemp < 900.0f && rtcMaxTemp > -900.0f) {
    display.printf("%.1f~%.1f\xB0" "C", rtcMinTemp, rtcMaxTemp);
  } else {
    display.print("---");
  }

  // 체감온도 (노랑)
  float heatIdx = calcHeatIndex(g_temperature, g_humidity);
  display.setTextColor(GxEPD_YELLOW);
  display.setCursor(6, 146);
  if (!isnan(heatIdx) && fabsf(heatIdx - g_temperature) > 0.5f) {
    display.printf("Feels %.1f\xB0" "C", heatIdx);
  } else if (!isnan(g_temperature)) {
    display.print("Feels OK");
  }

  drawHLine(0, 158, W, 1);

  // ══════════════════════════════════════════
  //  Humidity Section (y: 158 ~ 290)
  // ══════════════════════════════════════════
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(6, 180);
  display.print("HUMIDITY");

  display.setFont(&FreeSansBold24pt7b);
  display.setCursor(10, 224);
  if (!isnan(g_humidity)) {
    display.printf("%.1f%%", g_humidity);
  } else {
    display.print("--.-%");
  }

  // Min/Max
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(6, 246);
  if (rtcMinHum < 900.0f && rtcMaxHum > -900.0f) {
    display.printf("%.1f~%.1f%%", rtcMinHum, rtcMaxHum);
  } else {
    display.print("---");
  }

  // 습도 상태 (노랑)
  display.setTextColor(GxEPD_YELLOW);
  display.setCursor(6, 268);
  if (!isnan(g_humidity)) {
    if (g_humidity < 30.0f)      display.print("Too Dry");
    else if (g_humidity > 70.0f) display.print("Too Humid");
    else                         display.print("Comfortable");
  }

  drawHLine(0, 280, W, 2);

  // ══════════════════════════════════════════
  //  Bottom Info Section (y: 280 ~ 384)
  // ══════════════════════════════════════════
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  // 날짜/시간
  display.setCursor(6, 300);
  if (g_rtc_ok) {
    display.printf("%04d-%02d-%02d %02d:%02d",
      g_rtcNow.year, g_rtcNow.month, g_rtcNow.day,
      g_rtcNow.hour, g_rtcNow.minute);
  } else {
    display.print("RTC:FAIL");
  }

  // 센서/로거 상태
  display.setCursor(6, 318);
  display.printf("SHT:%s Log:%s",
    g_sht4_ok ? "OK" : "X",
    g_logger_ok ? "OK" : "X");

  // 부트카운트 / 배터리 추정
  display.setCursor(6, 336);
  display.printf("#%lu %dmin ~%dd",
    bootCount, SLEEP_INTERVAL_MIN, estimateBatteryDays());

  // WiFi 상태 (노랑)
  display.setTextColor(GxEPD_YELLOW);
  display.setCursor(6, 356);
  if (g_wifi_ok) {
    display.print("WiFi:Synced");
  } else {
    display.print("WiFi:Off");
  }

  // 보드명 (노랑, 맨 아래)
  display.setCursor(6, 376);
  display.print(BOARD_NAME);
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

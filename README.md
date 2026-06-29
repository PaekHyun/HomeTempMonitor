# 🏠 Home Temperature & Humidity Monitor

XIAO ESP32C6 + SHT40 + 2.9" E-Paper + DS3231 (내장 플래시 LittleFS 저장)

---

## 📋 프로젝트 개요

| 항목 | 내용 |
|------|------|
| MCU | Seeed XIAO ESP32C6 (ESP32-C6, 160MHz, 512KB SRAM, **4MB 내장 Flash**) |
| 온습도 센서 | Sensirion SHT40 (I2C, ±0.2°C, ±1.8% RH) |
| 디스플레이 | WeAct 2.9" Black-White-Red E-Paper (SPI, SSD1680, 128×296) |
| 데이터 저장 | **내장 플래시 LittleFS** (CSV 파일, ~1.5MB 가용) |
| RTC | DS3231 (I2C, ±2ppm, 내장 온도센서) |
| 배터리 | 10000mAh 리튬폴리머 |
| 통신 | WiFi 6 (NTP 시간동기화 + 웹서버 CSV 다운로드) |

---

## 🔌 배선도 (Wiring)

![Wiring Diagram](wiring_diagram.png)

### XIAO ESP32C6 핀 할당

```
XIAO ESP32C6          부품
─────────────          ────
D0  (GPIO0)  ────────  EPD BUSY
D1  (GPIO1)  ────────  EPD DC
D2  (GPIO2)  ────────  EPD RST
D3  (GPIO21) ────────  EPD CS
D4  (GPIO22) ────────  I2C SDA  ──┬── SHT40 SDA
D5  (GPIO23) ────────  I2C SCL  ──┼── DS3231 SDA
                                   ├── SHT40 SCL
                                   └── DS3231 SCL
D6  (GPIO16) ────────  (미사용)
D7  (GPIO17) ────────  (미사용)
D8  (GPIO19) ────────  SPI SCK  ──── EPD SCK (CLK)
D9  (GPIO20) ────────  SPI MISO (EPD 미사용, 예약)
D10 (GPIO18) ────────  SPI MOSI ──── EPD DIN (MOSI)
3V3            ────────  모든 모듈 VCC
GND            ────────  모든 모듈 GND
BAT            ────────  10000mAh Li-Po (+)
```

### 상세 연결표

| XIAO 핀 | GPIO | EPD 2.9" BWR | SHT40 | DS3231 |
|---------|------|-------------|-------|--------|
| D0      | 0    | BUSY        |       |        |
| D1      | 1    | DC          |       |        |
| D2      | 2    | RST         |       |        |
| D3      | 21   | CS          |       |        |
| D4      | 22   |             | SDA   | SDA    |
| D5      | 23   |             | SCL   | SCL    |
| D8      | 19   | SCK (CLK)   |       |        |
| D10     | 18   | DIN (MOSI)  |       |        |
| 3V3     |      | VCC         | VCC   | VCC    |
| GND     |      | GND         | GND   | GND    |

### I2C 풀업 저항
- SHT40과 DS3231이 같은 I2C 버스에 연결됨
- 두 모듈 모두 내부 풀업이 있으나, 안정성을 위해 **4.7kΩ 외부 풀업** 권장

---

## 📁 프로젝트 파일 구조

```
HomeTempMonitor/
├── HomeTempMonitor.ino   ← 메인 스케치 (진입점)
├── DataLogger.h          ← LittleFS 데이터 로거 헤더
├── DataLogger.cpp        ← LittleFS 데이터 로거 구현
├── RTClib_DS3231.h       ← DS3231 RTC 드라이버 헤더
├── RTClib_DS3231.cpp     ← DS3231 RTC 드라이버 구현
├── WebServer.h           ← WiFi 웹서버 헤더
├── WebServer.cpp         ← WiFi 웹서버 구현
├── wiring_diagram.svg    ← 배선도 (SVG)
├── wiring_diagram.png    ← 배선도 (PNG)
└── README.md             ← 이 파일
```

---

## 🛠️ Arduino IDE 설정

### 1. 보드 패키지 설치
- Arduino IDE → 파일 → 환경설정 → 추가 보드 매니저 URL:
  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```
- 툴 → 보드 → 보드 매니저 → **esp32 by Espressif Systems** 설치 (v3.0.0+)

### 2. 보드 선택
- 보드: **XIAO_ESP32C6**
- Upload Speed: 921600

### 3. ⚠️ 파티션 스킴 설정 (중요!)
- 툴 → Partition Scheme → **"Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"**
- 이 설정이 없으면 LittleFS에 데이터를 저장할 공간이 없습니다!

### 4. 라이브러리 설치
Arduino IDE → 스케치 → 라이브러리 포함하기 → 라이브러리 관리:

| 라이브러리 | 작성자 | 용도 |
|-----------|--------|------|
| GxEPD2 | Jean-Marc Capello | E-Paper 디스플레이 드라이버 |
| Adafruit SHT4x Library | Adafruit | SHT40 온습도 센서 |
| Adafruit GFX Library | Adafruit | GxEPD2 의존성 (자동 설치됨) |

---

## ⚙️ 설정 변경

`HomeTempMonitor.ino` 상단의 CONFIGURATION 섹션에서 수정:

```cpp
#define SLEEP_INTERVAL_MIN       5       // 측정 간격 (분)
#define WIFI_SYNC_INTERVAL_HR    24      // WiFi/NTP 동기화 주기 (시간)
#define WIFI_AP_TIMEOUT_SEC      120     // 웹서버 활성 시간 (초)
#define WIFI_SSID                "your_wifi_ssid"      // WiFi 이름
#define WIFI_PASSWORD            "your_wifi_password"   // WiFi 비밀번호
```

---

## 🔄 동작 흐름

```
[전원 ON / Deep Sleep 복귀]
         │
         ▼
  ┌──────────────┐
  │  I2C 초기화    │  SHT40 + DS3231
  │  센서 읽기     │  온도/습도 측정
  │  Min/Max 갱신  │  RTC 메모리 유지
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │  LittleFS     │  내장 플래시 마운트
  │  CSV 저장     │  /temp_log.csv 에 append
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │  E-Paper     │  온습도 + 시간 표시
  │  디스플레이    │  2.9" BWR 업데이트
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │  WiFi 필요?   │  24시간마다 또는 첫 부팅
  │  ┌─ YES ──┐  │
  │  │ NTP 동기화│  RTC 시간 보정
  │  │ 웹서버   │  120초간 CSV 다운로드 가능
  │  └────────┘  │
  └──────┬───────┘
         │
         ▼
  ┌──────────────┐
  │  Deep Sleep   │  ~15μA 소비
  │  5분 대기     │
  └──────────────┘
```

---

## 💾 데이터 저장 포맷 (내장 플래시 LittleFS)

### 저장 방식
- **CSV 파일** (`/temp_log.csv`) 로 내장 플래시 LittleFS 파티션에 저장
- 5분 간격 → 하루 288건 → 약 30KB/일
- **1.5MB 가용 공간** → 약 50일분 저장 가능
- 파일이 1MB 초과 시 자동으로 오래된 절반 삭제 (rotate)

### CSV 포맷
```csv
timestamp,temperature_c,humidity_pct
2026-06-30 07:20:00,24.56,45.23
2026-06-30 07:25:00,24.61,45.18
2026-06-30 07:30:00,24.58,45.30
```

### 내장 플래시 vs 외장 W25Q128 비교

| 항목 | 내장 플래시 (LittleFS) | 외장 W25Q128 |
|------|----------------------|-------------|
| 용량 | ~1.5MB (파티션에 따라) | 16MB |
| 저장 기간 | ~50일 (5분 간격) | ~20년 |
| 추가 부품 | 없음 | SPI 플래시 모듈 필요 |
| 배선 | 없음 | CS/SCK/MISO/MOSI 4선 |
| 전력 | 없음 (공유) | 추가 전력 소비 |
| 파일 시스템 | LittleFS (파일 단위) | Raw 바이너리 |
| 데이터 접근 | 파일 읽기/쓰기 | 주소 기반 접근 |

---

## 🌐 웹 서버 사용법

1. WiFi 동기화 시간에 ESP32가 WiFi에 연결됨
2. 시리얼 모니터에 표시된 IP 주소로 브라우저 접속
3. 웹 페이지에서:
   - **📥 Download CSV** 클릭 → `temp_data.csv` 다운로드
   - **📋 View JSON** 클릭 → JSON 형식으로 데이터 확인
   - **🗑️ Format Storage** → LittleFS 전체 삭제 (주의!)

---

## 🔋 배터리 수명 추정

| 모드 | 전류 | 시간/사이클 |
|------|------|------------|
| Deep Sleep | ~15 μA | 4분 50초 |
| Active (측정+표시+저장) | ~80 mA | ~10초 |
| WiFi (24시간에 1회) | ~120 mA | ~120초 |

**5분 간격 기준:**
- 평균 전류: ~0.29 mA
- **10000mAh 기준 약 154일 (약 5개월)**

---

## 🖥️ E-Paper 디스플레이 레이아웃

```
┌──────────────────────┐
│  Home Climate        │
│══════════════════════│
│                      │
│  TEMPERATURE (빨강)  │
│                      │
│    24.5°C            │
│                      │
│  22.1~26.3°C (빨강) │
│                      │
│──────────────────────│
│                      │
│  HUMIDITY            │
│                      │
│    45.2%             │
│                      │
│  40.0~55.2%          │
│                      │
│══════════════════════│
│  06-30 07:23         │
│  SHT:OK  Log:OK      │
│  #42  5min  ~154d    │
└──────────────────────┘
   128 × 296 (세로)
```

---

## ⚠️ 주의사항

1. **파티션 스킴 필수**: 반드시 "Default 4MB with spiffs" 선택! 기본 설정은 LittleFS 공간이 없습니다.

2. **첫 업로드 후 LittleFS 포맷**: 처음 업로드 시 LittleFS가 자동 포맷됩니다. 이후 재업로드해도 데이터는 유지됩니다.

3. **RTC 백업 배터리**: DS3231에 CR2032 코인셀을 장착하면 전원 차단 시에도 시간이 유지됩니다.

4. **WiFi 자격증명**: `WIFI_SSID`와 `WIFI_PASSWORD`를 반드시 수정하세요.

5. **3색 E-Paper**: 빨간색 업데이트는 흑백보다 시간이 오래 걸립니다 (~15초). 정상 동작입니다.

6. **Deep Sleep**: ESP32C6의 Deep Sleep은 완전한 리셋으로 복귀합니다. `setup()`이 매번 실행되며, `RTC_DATA_ATTR` 변수만 유지됩니다.

7. **저장 용량**: 1.5MB LittleFS 공간에 약 50일분 저장. 더 오래 보존하려면 주기적으로 CSV를 다운로드하세요.

---

## 📦 부품 목록 (BOM)

| 부품 | 수량 | 비고 |
|------|------|------|
| Seeed XIAO ESP32C6 | 1 | 메인 MCU (4MB 내장 Flash) |
| WeAct 2.9" BWR E-Paper | 1 | SSD1680, 128×296 |
| Sensirion SHT40 | 1 | I2C 온습도 센서 |
| DS3231 RTC 모듈 | 1 | I2C, ±2ppm |
| CR2032 코인셀 | 1 | DS3231 백업 (옵션) |
| 10000mAh Li-Po 배터리 | 1 | 3.7V |
| 4.7kΩ 저항 | 2 | I2C 풀업 (옵션) |
| 브레드보드/만능기판 | 1 | 연결용 |
| 점퍼 와이어 | 다수 | 연결용 |

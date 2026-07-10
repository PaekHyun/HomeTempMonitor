/*
 * DataLogger.cpp - LittleFS 기반 데이터 로거 구현
 */
#include "DataLogger.h"
#include <time.h>

DataLogger::DataLogger()
  : _initialized(false), _recordCount(0) {
}

bool DataLogger::begin() {
  if (!LittleFS.begin(true)) {  // true = format on fail
    Serial.println("[DataLogger] LittleFS mount FAILED!");
    _initialized = false;
    return false;
  }

  _initialized = true;
  countRecords();

  Serial.printf("[DataLogger] LittleFS mounted. Records: %lu\n", _recordCount);
  Serial.printf("[DataLogger] Total: %u bytes, Used: %u bytes\n",
    getTotalSpace(), getUsedSpace());
  return true;
}

bool DataLogger::logData(uint32_t unixtime, float temperature, float humidity) {
  if (!_initialized) return false;

  // 파일 크기 제한 확인
  if (getFileSize() >= LOG_MAX_SIZE) {
    Serial.println("[DataLogger] File size limit reached! Rotating...");
    // 가장 오래된 절반을 삭제하여 공간 확보
    rotateLog();
  }

  File file = LittleFS.open(LOG_FILE_PATH, "a");
  if (!file) {
    Serial.println("[DataLogger] Failed to open log file!");
    return false;
  }

  // 헤더가 없으면 작성
  if (file.size() == 0) {
    file.println("timestamp,temperature_c,humidity_pct");
  }

  // Unix timestamp → 읽을 수 있는 시간으로 변환
  // 주의: RTC에 이미 KST(UTC+9)로 저장되어 있으므로
  // unixtime()도 KST 기준이며, 추가 보정 없이 그대로 변환
  time_t rawtime = unixtime;
  struct tm *timeinfo = gmtime(&rawtime);
  char timeBuf[25];
  strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", timeinfo);

  file.printf("%s,%.2f,%.2f\n", timeBuf, temperature, humidity);
  file.close();

  _recordCount++;
  return true;
}

bool DataLogger::readAllCSV(Stream &out) {
  if (!_initialized) return false;

  File file = LittleFS.open(LOG_FILE_PATH, "r");
  if (!file) {
    out.println("timestamp,temperature_c,humidity_pct");
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      out.println(line);
    }
  }

  file.close();
  return true;
}

bool DataLogger::readAllJSON(Stream &out) {
  if (!_initialized) return false;

  File file = LittleFS.open(LOG_FILE_PATH, "r");
  if (!file) return false;

  // 첫 줄(헤더) 건너뛰기
  String header = file.readStringUntil('\n');

  out.printf("{\"recordCount\":%lu,\"records\":[", _recordCount);

  bool first = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // CSV 파싱: timestamp,temp,hum
    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    if (comma1 < 0 || comma2 < 0) continue;

    String ts   = line.substring(0, comma1);
    String temp = line.substring(comma1 + 1, comma2);
    String hum  = line.substring(comma2 + 1);

    if (!first) out.print(",");
    first = false;

    out.printf("{\"ts\":\"%s\",\"temp\":%s,\"hum\":%s}",
      ts.c_str(), temp.c_str(), hum.c_str());
  }

  out.println("]}");
  file.close();
  return true;
}

bool DataLogger::format() {
  LittleFS.format();
  _recordCount = 0;
  Serial.println("[DataLogger] LittleFS formatted.");
  return true;
}

uint32_t DataLogger::getRecordCount() {
  return _recordCount;
}

size_t DataLogger::getFileSize() {
  if (!_initialized) return 0;
  File file = LittleFS.open(LOG_FILE_PATH, "r");
  if (!file) return 0;
  size_t size = file.size();
  file.close();
  return size;
}

size_t DataLogger::getTotalSpace() {
  return LittleFS.totalBytes();
}

size_t DataLogger::getUsedSpace() {
  return LittleFS.usedBytes();
}

bool DataLogger::isAvailable() {
  return _initialized;
}

bool DataLogger::countRecords() {
  _recordCount = 0;

  File file = LittleFS.open(LOG_FILE_PATH, "r");
  if (!file) return true; // 파일 없음 = 0건

  // 첫 줄(헤더) 건너뛰기
  file.readStringUntil('\n');

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 1) {  // 빈 줄 제외
      _recordCount++;
    }
  }

  file.close();
  return true;
}

bool DataLogger::rotateLog() {
  // 파일의 뒤쪽 절반만 유지
  File file = LittleFS.open(LOG_FILE_PATH, "r");
  if (!file) return false;

  // 전체 내용을 메모리에 읽기
  // (LittleFS 파일이 1MB 이하이므로 메모리에 충분히 적재 가능)
  String allLines = file.readString();
  file.close();

  // 줄 단위로 분할
  int totalLines = 0;
  int idx = 0;
  while ((idx = allLines.indexOf('\n', idx)) >= 0) {
    totalLines++;
    idx++;
  }

  // 뒤쪽 절반만 유지
  int keepFrom = totalLines / 2;
  
  file = LittleFS.open(LOG_FILE_PATH, "w");
  if (!file) return false;

  file.println("timestamp,temperature_c,humidity_pct");

  idx = 0;
  int lineNum = 0;
  int start = 0;
  while (start < allLines.length()) {
    int end = allLines.indexOf('\n', start);
    if (end < 0) end = allLines.length();
    String line = allLines.substring(start, end);
    line.trim();
    if (lineNum >= keepFrom && line.length() > 0 && line.indexOf("timestamp") < 0) {
      file.println(line);
    }
    lineNum++;
    start = end + 1;
  }

  file.close();
  countRecords();
  return true;
}

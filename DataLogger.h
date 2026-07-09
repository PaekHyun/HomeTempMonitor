/*
 * DataLogger.h - LittleFS 기반 데이터 로거 (내장 플래시 사용)
 * 
 * XIAO ESP32C6의 내장 4MB 플래시 중 LittleFS 파티션에
 * 온습도 데이터를 CSV로 저장합니다.
 * 
 * 파티션 레이아웃 (boards.txt 커스텀 필요):
 *   - 0x000000 ~ : Bootloader
 *   - 0x00xxxx ~ : Partitions table
 *   - 0x00xxxx ~ : App (1.9MB)
 *   - 0x3D0000 ~ : LittleFS (spiffs, ~200KB)
 * 
 * 또는 기본 파티션 사용 시:
 *   - App: ~1.3MB, LittleFS: ~1.5MB
 */
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>

#define LOG_FILE_PATH  "/temp_log.csv"
#define LOG_MAX_SIZE   1048576  // 1MB 제한 (여유 공간 보존)

class DataLogger {
public:
  DataLogger();

  bool begin();
  bool logData(uint32_t unixtime, float temperature, float humidity);
  bool readAllCSV(Stream &out);
  bool readAllJSON(Stream &out);
  bool format();
  uint32_t getRecordCount();
  size_t getFileSize();
  size_t getTotalSpace();
  size_t getUsedSpace();
  bool isAvailable();

private:
  bool _initialized;
  uint32_t _recordCount;
  
  bool countRecords();
  bool rotateLog();
};

#endif // DATA_LOGGER_H

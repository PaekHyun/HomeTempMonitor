/*
 * WebServer.cpp - WiFi 웹서버 구현 (LittleFS 기반)
 */
#include "WebServer.h"
#include <time.h>

DataWebServer::DataWebServer(DataLogger &logger)
  : _logger(logger), _server(80), _running(false), _timeSyncCallback(nullptr) {
}

void DataWebServer::start(const char *ssid, const char *password) {
  Serial.printf("[WebServer] Connecting to WiFi: %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WebServer] WiFi connection FAILED!");
    _running = false;
    return;
  }

  Serial.println("[WebServer] WiFi connected! IP: " + WiFi.localIP().toString());

  // NTP 동기화
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  struct tm timeinfo;
  int ntp_tries = 0;
  while (!getLocalTime(&timeinfo) && ntp_tries < 15) {
    delay(500);
    ntp_tries++;
  }
  if (getLocalTime(&timeinfo)) {
    Serial.printf("[WebServer] NTP synced: %s", asctime(&timeinfo));
    if (_timeSyncCallback) _timeSyncCallback();
  }

  _server.begin();
  _running = true;
}

void DataWebServer::stop() {
  _server.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  _running = false;
  Serial.println("[WebServer] Stopped.");
}

bool DataWebServer::isRunning() {
  return _running;
}

void DataWebServer::handleClient() {
  if (!_running) return;

  WiFiClient client = _server.available();
  if (!client) return;

  String request = "";
  unsigned long timeout = millis() + 3000;
  while (client.connected() && millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (request.endsWith("\r\n\r\n")) break;
    }
  }

  if (request.length() > 0) {
    handleRequest(client, request);
  }
  
  delay(10);
  client.stop();
}

void DataWebServer::handleRequest(WiFiClient &client, const String &request) {
  Serial.println("[WebServer] Request: " + request.substring(0, request.indexOf('\r')));

  if (request.indexOf("GET / ") >= 0 || request.indexOf("GET /index.html") >= 0) {
    sendHTML(client);
  }
  else if (request.indexOf("GET /data.csv") >= 0) {
    sendCSV(client);
  }
  else if (request.indexOf("GET /data.json") >= 0) {
    sendJSON(client);
  }
  else if (request.indexOf("GET /format") >= 0) {
    _logger.format();
    sendHTML(client);
  }
  else {
    send404(client);
  }
}

void DataWebServer::sendHTML(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=utf-8");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html><html><head>");
  client.println("<meta charset='utf-8'>");
  client.println("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  client.println("<title>Home Temp Monitor</title>");
  client.println("<style>");
  client.println("body{font-family:system-ui,sans-serif;max-width:600px;margin:40px auto;padding:0 20px;background:#f5f5f5}");
  client.println("h1{color:#333;border-bottom:2px solid #4CAF50;padding-bottom:10px}");
  client.println(".card{background:#fff;border-radius:8px;padding:20px;margin:15px 0;box-shadow:0 2px 4px rgba(0,0,0,0.1)}");
  client.println(".btn{display:inline-block;padding:12px 24px;margin:5px;border-radius:6px;text-decoration:none;color:#fff;font-weight:bold}");
  client.println(".btn-green{background:#4CAF50}.btn-red{background:#f44336}.btn-blue{background:#2196F3}");
  client.println(".info{color:#666;font-size:14px}");
  client.println(".warn{color:#f44336;font-weight:bold}");
  client.println("</style></head><body>");
  client.println("<h1>🏠 Home Temp Monitor</h1>");

  client.println("<div class='card'>");
  client.printf("<h2>📊 Data Records: %lu</h2>", _logger.getRecordCount());
  client.printf("<p class='info'>File size: %u bytes</p>", _logger.getFileSize());
  client.printf("<p class='info'>Flash: %u / %u bytes used</p>",
    _logger.getUsedSpace(), _logger.getTotalSpace());
  client.println("<br>");
  client.println("<a href='/data.csv' class='btn btn-green'>📥 Download CSV</a>");
  client.println("<a href='/data.json' class='btn btn-blue'>📋 View JSON</a>");
  client.println("</div>");

  client.println("<div class='card'>");
  client.println("<h2>⚙️ Settings</h2>");
  client.println("<a href='/format' class='btn btn-red' onclick=\"return confirm('Erase ALL data?')\">🗑️ Format Storage</a>");
  client.println("<p class='warn'>⚠️ Format erases all stored data permanently!</p>");
  client.println("</div>");

  client.println("<div class='card'>");
  client.println("<h2>ℹ️ Info</h2>");
  client.println("<p class='info'>• Sensor: SHT40 (I2C)</p>");
  client.println("<p class='info'>• Display: 2.9\" E-Paper BWR (SPI)</p>");
  client.println("<p class='info'>• Storage: Internal Flash (LittleFS)</p>");
  client.println("<p class='info'>• RTC: DS3231 (I2C)</p>");
  client.println("<p class='info'>• Interval: 5 minutes</p>");
  client.println("<p class='info'>• Battery: 10000mAh Li-Po</p>");
  client.println("</div>");

  client.println("</body></html>");
}

void DataWebServer::sendCSV(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/csv; charset=utf-8");
  client.println("Content-Disposition: attachment; filename=\"temp_data.csv\"");
  client.println("Connection: close");
  client.println();

  _logger.readAllCSV(client);
}

void DataWebServer::sendJSON(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json; charset=utf-8");
  client.println("Connection: close");
  client.println();

  _logger.readAllJSON(client);
}

void DataWebServer::send404(WiFiClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/plain");
  client.println("Connection: close");
  client.println();
  client.println("404 - Not Found");
}

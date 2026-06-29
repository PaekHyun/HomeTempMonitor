/*
 * WebServer.h - WiFi 웹서버 (LittleFS 기반)
 */
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include "DataLogger.h"

class DataWebServer {
public:
  DataWebServer(DataLogger &logger);
  
  void start(const char *ssid, const char *password);
  void stop();
  void handleClient();
  bool isRunning();
  
  void onTimeSync(void (*callback)()) { _timeSyncCallback = callback; }
  
private:
  DataLogger &_logger;
  WiFiServer  _server;
  bool        _running;
  void (*_timeSyncCallback)();
  
  void handleRequest(WiFiClient &client, const String &request);
  void sendHTML(WiFiClient &client);
  void sendCSV(WiFiClient &client);
  void sendJSON(WiFiClient &client);
  void send404(WiFiClient &client);
};

#endif // WEBSERVER_H

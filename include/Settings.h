#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <GyverRelay.h>
#include <GyverDS18Array.h>
#include <GyverSegment.h>
#include <LittleFS.h>
#include <time.h>

#define DS_PIN 5
#define CLK_PIN 4
#define DIO_PIN 2
#define RELAY1_PIN 18
#define RELAY2_PIN 19

extern volatile float targetTemp;
extern float tempIn[5]; 
extern float tempOut;
extern bool relay1Stat;
extern bool relay2Stat;
extern float historyRT[30];      // История средней темп. (RT)
extern float historyRT_Out[30];  // История уличной темп. (RT)
extern portMUX_TYPE sharedDataMux;
extern bool connectStat = 0;

String ver, notes;
bool updateAvailable = false;

extern WebServer server;
extern Disp1637Colon disp;
extern GyverRelay regs[4];

// Добавьте в Settings.h
struct WiFiConfig {
    char ssid[32];
    char pass[64];
};

extern WiFiConfig wcfg;
void loadWiFiConfig();
void saveWiFiConfig(const char* s, const char* p);

#endif

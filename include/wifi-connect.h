#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include "Settings.h"

WiFiConfig wcfg;

void loadWiFiConfig() {
    if (LittleFS.exists("/wifi.dat")) {
        File f = LittleFS.open("/wifi.dat", FILE_READ);
        f.read((uint8_t*)&wcfg, sizeof(wcfg));
        f.close();
    } else {
        // Дефолтные значения из вашего кода
        strncpy(wcfg.ssid, "DTT-231/2", 32);
        strncpy(wcfg.pass, "#231-akv0", 64);
    }
}

void saveWiFiConfig(const char* s, const char* p) {
    strncpy(wcfg.ssid, s, 32);
    strncpy(wcfg.pass, p, 64);
    File f = LittleFS.open("/wifi.dat", FILE_WRITE);
    f.write((uint8_t*)&wcfg, sizeof(wcfg));
    f.close();
}

void startConfigPortal() {
    WiFi.softAP("ESP32_Config_Portal");
    Serial.println("Запущена точка доступа: ESP32_Config_Portal");

    connectStat = 1;
    
    // Временный маршрут для сохранения данных
    server.on("/save_wifi", HTTP_POST, []() {
        if (server.hasArg("s") && server.hasArg("p")) {
            saveWiFiConfig(server.arg("s").c_str(), server.arg("p").c_str());
            server.send(200, "text/html", "Готово! ESP32 перезагружается...");
            delay(2000);
            ESP.restart();
        }
    });
    
    // Простая страница настройки
    server.on("/config", HTTP_GET, []() {
        String h = "<html><form method='POST' action='/save_wifi'>";
        h += "SSID: <input name='s' value='" + String(wcfg.ssid) + "'><br>";
        h += "PASS: <input name='p' type='password'><br>";
        h += "<input type='submit' value='Save'></form></html>";
        server.send(200, "text/html", h);
    });
}

#endif

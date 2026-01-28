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
    WiFi.softAP("ESP32");
    Serial.println("Запущена точка доступа: ESP32");

    connectStat = 1;
    
    // Временный маршрут для сохранения данных
    server.on("/save_wifi", HTTP_POST, []() {
        if (server.hasArg("s") && server.hasArg("p")) {
            saveWiFiConfig(server.arg("s").c_str(), server.arg("p").c_str());
            server.send(200, "text/html", "<!DOCTYPE html><html><meta charset='utf-8'><body style='background:#121212;color:#00e676;text-align:center;padding-top:50px;font-family:sans-serif;'>"
                                      "<h2>Настройки сохранены!</h2><p>ESP32 перезагружается...</p></body></html>");
            delay(5000);
            ESP.restart();
        }
    });
    
    // Простая страница настройки
    server.on("/config", HTTP_GET, []() {
        String html = F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
        html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
        html += F("<style>");
        // Используем те же стили, что и в основном интерфейсе
        html += F("body{font-family:sans-serif;background:#121212;color:#e0e0e0;text-align:center;margin:0;padding:20px;}");
        html += F(".card{background:#1e1e1e;padding:20px;border-radius:12px;max-width:400px;margin:auto;box-shadow:0 4px 10px rgba(0,0,0,0.5);border:1px solid #333;}");
        html += F("input{width:100%;background:#333;color:#fff;border:1px solid #444;padding:10px;margin:10px 0;border-radius:5px;box-sizing:border-box;font-size:1em;}");
        html += F(".btn-save{width:100%;background:#00e676;color:#000;border:none;padding:12px;border-radius:5px;font-weight:bold;cursor:pointer;font-size:1em;}");
        html += F(".btn-back{display:inline-block;margin-top:15px;color:#888;text-decoration:none;font-size:0.9em;}");
        html += F("h2{color:#00e676;margin-top:0;}");
        html += F("</style></head><body>");

        html += F("<div class='card'><h2>Настройка Wi-Fi</h2>");
        html += F("<p style='font-size:0.9em;color:#888;'>Введите данные вашей сети. После сохранения устройство перезагрузится.</p>");
        html += F("<form method='POST' action='/save_wifi'>");
        html += "<input name='s' placeholder='SSID (Имя сети)' value='" + String(wcfg.ssid) + "' required>";
        html += F("<input name='p' type='password' placeholder='Пароль' required>");
        html += F("<button class='btn-save' type='submit'>СОХРАНИТЬ И ПЕРЕЗАГРУЗИТЬ</button>");
        html += F("</form>");
        html += F("<a href='/' class='btn-back'>← Вернуться назад</a>");
        html += F("</div></body></html>");
        server.send(200, "text/html", html);
    });
}
#endif

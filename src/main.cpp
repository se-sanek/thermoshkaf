#define VERSION "1.0.3"

#include <Arduino.h>
#include <AutoOTA.h> // Добавляем инклуд
#include "Settings.h"
#include "WebInterface.h"
#include "DisplayLogic.h"

// Создаем объект OTA
AutoOTA ota(VERSION, "se-sanek/thermoshkaf");

// Переменные
volatile float targetTemp = 25.0;
float tempIn[5] = {0};
float tempOut = 0;
bool relay1Stat = false;
bool relay2Stat = false;
float historyRT[30] = {0};
float historyRT_Out[30] = {0}; // Новый массив
portMUX_TYPE sharedDataMux = portMUX_INITIALIZER_UNLOCKED;


WebServer server(80);

Disp1637Colon disp(DIO_PIN, CLK_PIN, 1);

uint64_t sensorAddr[] = {
    0x1F000000526CA628, 0xA300000051EF3628,
    0xAB0000005393F228, 0x730000007241EA28,
    0x520000007CAAAF28
};

GyverDS18Array ds(DS_PIN, sensorAddr, 5);
GyverRelay regs[4] = { REVERSE, REVERSE, REVERSE, REVERSE };


void cleanupOldLogs() {
    Serial.println("Проверка старых логов...");
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("NTP время не получено, очистка отложена.");
        return;
    }

    time_t now;
    time(&now);
    const long secondsIn30Days = 30 * 24 * 60 * 60; // 30 дней в секундах

    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {
        String fileName = file.name();
        
        // Файлы имеют формат /YYYY-MM-DD.txt (длина 15 символов с учетом /)
        if (fileName.endsWith(".txt") && fileName.length() >= 11) {
            // Пытаемся извлечь дату из имени файла /2026-01-16.txt
            // Формат подстроки зависит от того, возвращает ли file.name() путь с косой чертой или без
            int offset = fileName.startsWith("/") ? 1 : 0;
            
            int year = fileName.substring(offset, offset + 4).toInt();
            int month = fileName.substring(offset + 5, offset + 7).toInt();
            int day = fileName.substring(offset + 8, offset + 10).toInt();

            if (year > 2000) { // Простая проверка на валидность даты
                struct tm fileTimeStruct = {0};
                fileTimeStruct.tm_year = year - 1900;
                fileTimeStruct.tm_mon = month - 1;
                fileTimeStruct.tm_mday = day;
                fileTimeStruct.tm_hour = 12; // Сравниваем по середине дня

                time_t fileTime = mktime(&fileTimeStruct);

                if (now - fileTime > secondsIn30Days) {
                    String pathToDelete = fileName.startsWith("/") ? fileName : "/" + fileName;
                    file.close(); // Обязательно закрываем файл перед удалением
                    LittleFS.remove(pathToDelete);
                    Serial.print("Удален старый файл: ");
                    Serial.println(pathToDelete);
                    
                    // После удаления структура директории может измениться, 
                    // поэтому лучше начать обход заново или использовать массив имен
                    root = LittleFS.open("/"); 
                }
            }
        }
        file = root.openNextFile();
    }
    Serial.println("Очистка завершена.");
}

// Новая логика записи: Час, Внутренняя, Уличная
void logHourlyData(float tIn, float tOut) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    if (timeinfo.tm_min != 0) return;
    static int lastHour = -1;
    if (timeinfo.tm_hour != lastHour) {
        Serial.print("Write data to time: ");
        Serial.print(timeinfo.tm_hour);
        Serial.print(" : ");
        Serial.print(timeinfo.tm_min);
        Serial.print(" : ");
        Serial.println(timeinfo.tm_year+1900);
        lastHour = timeinfo.tm_hour;
        // --- ДОБАВЛЕНО: Запуск очистки раз в сутки в 00:00 ---
        if (timeinfo.tm_hour == 0) {
            cleanupOldLogs();
        }
        // ---------------------------------------------------
        char fileName[32];
        strftime(fileName, sizeof(fileName), "/%Y-%m-%d.txt", &timeinfo);Serial.print("Full path to write: "); Serial.println(fileName);
        File file = LittleFS.open(fileName, FILE_APPEND);
        if (file) {
            file.printf("%d,%.1f,%.1f\n", timeinfo.tm_hour, tIn, tOut);
            file.close();
            Serial.println("File write success");
        }
    }
}

void networkTask(void* p) {
    for (;;) {
        server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}void saveTargetTemp(float temp) {
    File file = LittleFS.open("/target.txt", FILE_WRITE);
    if (file) {
        file.print(temp);
        file.close();
        Serial.println("Уставка сохранена в LittleFS: " + String(temp));
    }
}

void loadTargetTemp() {
    if (LittleFS.exists("/target.txt")) {
        File file = LittleFS.open("/target.txt", FILE_READ);
        if (file) {
            String val = file.readString();
            float loadedTemp = val.toFloat();
            if (loadedTemp >= 5 && loadedTemp <= 40) { // Проверка на корректность
                targetTemp = loadedTemp;
                Serial.println("Уставка загружена из файла: " + String(targetTemp));
            }
            file.close();
        }
    } else {
        Serial.println("Файл уставки не найден, используем значение по умолчанию");
        saveTargetTemp(targetTemp); // Создаем файл с начальным значением
    }
}

void setup() {
    Serial.begin(115200);
    
    // 1. Инициализация файловой системы
    if (!LittleFS.begin(true)) {
        Serial.println("Ошибка LittleFS!");
    }

    // 2. Загрузка сохраненной температуры СРАЗУ после LittleFS
    loadTargetTemp();

    WiFi.begin("DTT-231/2", "#231-akv1");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");
    Serial.println(WiFi.localIP());

    configTime(18000, 0, "ntp0.ntp-servers.net");

    server.on("/", HTTP_GET, [](){ server.send(200, "text/html", getPage()); });
    server.on("/get_data", HTTP_GET, handleData);
    server.on("/get_history", HTTP_GET, handleGetHistory);
    server.on("/set", HTTP_POST, [](){
        if (server.hasArg("temp")) {
            float val = server.arg("temp").toFloat();
            if (val >= 0 && val <= 40) {
                portENTER_CRITICAL(&sharedDataMux);
                targetTemp = val;
                portEXIT_CRITICAL(&sharedDataMux);
                
                // СОХРАНЯЕМ В ФАЙЛ
                saveTargetTemp(targetTemp);
            }
        }
        server.send(200, "text/plain", "OK");
    });
    server.on("/check_update", HTTP_GET, [](){
        ota.checkUpdate(&ver, &notes); 
        server.send(200, "text/plain", "OK");
    });
    // Маршрут для запуска самого обновления
    server.on("/do_ota", HTTP_GET, [](){
        server.send(200, "text/plain", "Update...");
        ota.updateNow(); // Запуск загрузки и прошивки
    });
    // Добавьте в setup()
    server.on("/ota_page", HTTP_GET, [](){
        server.send(200, "text/html", getOTAPage(updateAvailable, ver, notes));
    });

    server.on("/check_manual", HTTP_GET, [](){
        // Запускаем проверку
        if (ota.checkUpdate(&ver, &notes)) {
            updateAvailable = true;
        } else {
            updateAvailable = false;
        }
        // После проверки перенаправляем пользователя обратно на ту же страницу
        server.sendHeader("Location", "/ota_page");
        server.send(303);
    });

    server.on("/do_ota", HTTP_GET, [](){
        if (updateAvailable) {
            server.send(200, "text/html", "<h2>Обновление запущено...</h2><p>ESP32 перезагрузится через 1-2 минуты.</p>");
            ota.update(); 
        } else {
            server.send(200, "text/plain", "No update found");
        }
    });
    server.begin();

    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);

    xTaskCreatePinnedToCore(networkTask, "NetTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
    if (!ds.tick()) {
        float temps[5];
        ds.readTemps(temps);
        
        float sum = 0; int count = 0;
        for (int i = 0; i < 4; i++) {
            if (!isnan(temps[i])) { 
              tempIn[i] = temps[i]; 
              sum += temps[i]; 
              count++; }
        }
        if (count > 0) tempIn[4] = sum / count;
        tempOut = temps[4];

        // Сдвигаем оба графика RT
        for (int i = 0; i < 29; i++) {
            historyRT[i] = historyRT[i+1];
            historyRT_Out[i] = historyRT_Out[i+1];
        }
        historyRT[29] = tempIn[4];
        historyRT_Out[29] = tempOut;

        // Логируем две температуры
        logHourlyData(tempIn[4], tempOut);

        for (int i = 0; i < 4; i++) {
            regs[i].setpoint = targetTemp;
            regs[i].input = tempIn[i];
        }
        
        relay1Stat = (regs[0].getResultTimer() || regs[1].getResultTimer() || regs[2].getResultTimer());
        relay2Stat = regs[3].getResultTimer();
        
        digitalWrite(RELAY1_PIN, !relay1Stat);
        digitalWrite(RELAY2_PIN, !relay2Stat);
        
    }
    updateDisplay1();
}
#include <Arduino.h>
#include "Settings.h"
#include "WebInterface.h"
#include "DisplayLogic.h"

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

// Новая логика записи: Час, Внутренняя, Уличная
void logHourlyData(float tIn, float tOut) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    static int lastHour = -1;
    if (timeinfo.tm_hour != lastHour) {
        lastHour = timeinfo.tm_hour;
        char fileName[32];
        strftime(fileName, sizeof(fileName), "/%Y-%m-%d.txt", &timeinfo);
        File file = LittleFS.open(fileName, FILE_APPEND);
        if (file) {
            file.printf("%d,%.1f,%.1f\n", timeinfo.tm_hour, tIn, tOut);
            file.close();
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
    while (WiFi.status() != WL_CONNECTED) delay(500);
    configTime(10800, 0, "pool.ntp.org");

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
    updateDisplay();
}
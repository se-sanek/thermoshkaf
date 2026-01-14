#include <WiFi.h>
#include <WebServer.h>
#include <GyverDS18Array.h>   // Установите библиотеку GyverDS18 (не microDS18)
#include <GyverSegment.h>
#include <GyverRelay.h>

// ================= НАСТРОЙКИ СЕТИ =================
const char* ssid = "DTT-231/2";
const char* password = "#231-akv1";

// ================= ПИНЫ =================
#define DS_PIN 13            // Шина датчиков
#define CLK_PIN 2           // Дисплей
#define DIO_PIN 4           // Дисплей
#define RELAY_PIN 18        // Реле

// ================= ОБЪЕКТЫ =================
Disp1637Colon disp(CLK_PIN, DIO_PIN);
GyverRelay regulator(REVERSE);
WebServer server(80);

// ================= ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ =================
// Данные датчиков
#define MAX_SENSORS 4
uint64_t sensorAddrs[] = {
    0x4D0417508099FF28, //датчик на дне
    0xFE04175159CDFF28, //датчик воздуха в центре
    0xC3041750E553FF28, //датчик наверху
    0xCF0417505B78FF28, //датчик у двери
}; // Массив адресов
float sensorTemps[MAX_SENSORS];    // Массив температур
int deviceCount = MAX_SENSORS;               // Реальное кол-во датчиков

GyverDS18 ds(DS_PIN);       // Объект для работы с шиной

volatile float currentAvgTemp = 0.0;
volatile float targetTemp = 25.0;

// Мьютекс для безопасного доступа к переменным из разных ядер
portMUX_TYPE sharedDataMux = portMUX_INITIALIZER_UNLOCKED;

// ================= ЗАДАЧА СЕТИ (ЯДРО 0) =================
void networkTask(void * parameter) {
  while (true) {
    server.handleClient();
    vTaskDelay(2); // Обязательно уступаем время, иначе watchdog сработает
  }
}

// ================= HTML СТРАНИЦА =================
String getPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Climate</title>";
  html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}";
  html += ".card{background:#eee;padding:20px;margin:10px auto;max-width:400px;border-radius:10px;}";
  html += "button{padding:10px 20px;font-size:18px;}</style></head><body>";
  
  html += "<h1>Термостат</h1>";
  
  // Читаем переменные с блокировкой (на всякий случай, хотя float атомарен на 32бит)
  float avg, trg;
  portENTER_CRITICAL(&sharedDataMux);
  avg = currentAvgTemp;
  trg = targetTemp;
  portEXIT_CRITICAL(&sharedDataMux);

  html += "<div class='card'><h2>Средняя: " + String(avg, 1) + " C</h2>";
  html += "<p>Цель: <b>" + String(trg, 1) + "</b></p></div>";

  html += "<div class='card'><h3>Датчики</h3>";
  for(int i=0; i<deviceCount; i++) {
     html += "D" + String(i+1) + ": <b>" + String(sensorTemps[i], 1) + "</b> C<br>";
  }
  html += "</div>";

  html += "<div class='card'><form action='/set' method='POST'>";
  html += "Установить: <input type='number' step='0.1' name='temp' value='" + String(trg, 1) + "'> ";
  html += "<button type='submit'>OK</button></form></div>";
  
  html += "</body></html>";
  return html;
}

void handleRoot() {
  server.send(200, "text/html", getPage());
}

void handleSet() {
  if (server.hasArg("temp")) {
    float t = server.arg("temp").toFloat();
    portENTER_CRITICAL(&sharedDataMux);
    targetTemp = t;
    portEXIT_CRITICAL(&sharedDataMux);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// ================= SETUP (ЯДРО 1) =================
void setup() {
  Serial.begin(9600);

  // Настройка пинов
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  // Дисплей
  //disp.displayByte(_S, _c, _a, _n);

  // Настройка WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Запуск сервера
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_POST, handleSet);
  server.begin();

  // Запускаем задачу сети на Ядре 0
  xTaskCreatePinnedToCore(
    networkTask,   // Функция
    "NetTask",     // Имя
    5000,          // Стек
    NULL,          // Параметры
    1,             // Приоритет
    NULL,          // Хендл
    0              // ЯДРО 0 (Здесь будет крутиться WebServer)
  );

  // Настройка регулятора
  regulator.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  regulator.hysteresis = 2;   // ширина гистерезиса
  regulator.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  //regulator.hysteresis = 0.5;
  //regulator.setDirection(NORMAL);
}

// ================= LOOP (ЯДРО 1) - ЛОГИКА =================
void loop() {
  // 1. tick() должен вызываться постоянно. 
  // Он сам следит за таймерами (запрос/чтение)
  // Возвращает true ТОЛЬКО в тот момент, когда данные ВСЕХ датчиков обновились
  if (ds.ready()) {
    float sum = 0;
    int valid = 0;

    for (int i = 0; i < 4; i++) {
      float t;
      if(ds.readTemp(sensorAddrs[i])){
        t = ds.getTemp();
      }
      
      if (t > -50) { // Проверка на корректность (не ошибка)
        sensorTemps[i] = t;
        sum += t;
        valid++;
        Serial.print("Sensor "); Serial.print(i); 
        Serial.print(": "); Serial.println(t);
      }
    }
    ds.requestTemp();

    // Считаем среднее только если есть живые датчики
    if (valid > 0) {
      portENTER_CRITICAL(&sharedDataMux);
      currentAvgTemp = sum / valid; // ПРАВИЛЬНЫЙ расчет среднего
      portEXIT_CRITICAL(&sharedDataMux);
    }

    // Обновляем дисплей
    //disp.displayInt((int)round(currentAvgTemp));
    
    Serial.print("Average: ");
    Serial.println(currentAvgTemp);
  }

  // 2. Управление реле (выносим из условия tick, чтобы регулятор работал чаще)
  // Но входные данные обновятся только когда сработает ds.tick()
  portENTER_CRITICAL(&sharedDataMux);
  regulator.input = currentAvgTemp;
  regulator.setpoint = targetTemp;
  portEXIT_CRITICAL(&sharedDataMux);
  
  // Управляем реле (инверсия ! если модуль Low Level)
  digitalWrite(RELAY_PIN, !regulator.getResult());
}
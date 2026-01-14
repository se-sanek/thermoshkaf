#include <WiFi.h>
#include <WebServer.h>
#include <GyverDS18Array.h>   // Установите библиотеку GyverDS18 (не microDS18)
#include <GyverSegment.h>
#include <GyverRelay.h>

// ================= НАСТРОЙКИ СЕТИ =================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASS";

// ================= ПИНЫ =================
#define DS_PIN 5            // Шина датчиков
#define CLK_PIN 2           // Дисплей
#define DIO_PIN 4           // Дисплей
#define RELAY_PIN 18        // Реле

// ================= ОБЪЕКТЫ =================
GyverSegment disp(CLK_PIN, DIO_PIN);
GyverRelay regulator(RELE_COMMON);
WebServer server(80);

// ================= ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ =================
// Данные датчиков
#define MAX_SENSORS 4
uint64_t sensorAddrs[MAX_SENSORS] = {
    0x4D0417508099FF28, //датчик на дне
    0xFE04175159CDFF28, //датчик воздуха в центре
    0xC3041750E553FF28, //датчик наверху
    0xCF0417505B78FF28, //датчик у двери
}; // Массив адресов
float sensorTemps[MAX_SENSORS];    // Массив температур
int deviceCount = MAX_SENSORS;               // Реальное кол-во датчиков
GyverDS18Array ds(DS_PIN, sensorAddrs, MAX_SENSORS);       // Объект для работы с шиной

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
  disp.brightness(7);
  disp.displayByte(_S, _c, _a, _n);

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
  regulator.setHb(0.5);
  regulator.setDirection(NORMAL);
}

// ================= LOOP (ЯДРО 1) - ЛОГИКА =================
void loop() {
  static uint32_t tmr;
  
  // Опрос раз в секунду
  if (millis() - tmr >= 1000) {
    tmr = millis();

    // 1. Запрос температуры сразу всем датчикам (SKIP ROM)
    // GyverDS18 не имеет явного requestAll, но можно послать request
    // по очереди или использовать broadcast логику, если библиотека позволяет.
    // Самый надежный способ с адресами:
    for (int i = 0; i < deviceCount; i++) {
      ds.request(sensorAddrs[i]);
    }
    
    // Ждем конвертации (DS18B20 до 750мс на 12 бит)
    // Так как мы в loop(), лучше не делать delay(750), но для простоты
    // и так как сеть на другом ядре, здесь это допустимо.
    vTaskDelay(800 / portTICK_PERIOD_MS); 

    // 2. Чтение данных
    float sum = 0;
    int valid = 0;
    
    for (int i = 0; i < deviceCount; i++) {
      if (ds.ready(sensorAddrs[i])) {
        float t = ds.getTemp(sensorAddrs[i]);
        sensorTemps[i] = t; // Обновляем массив для веб-страницы
        sum += t;
        valid++;
      }
    }

    // 3. Расчет и Регулятор
    if (valid > 0) {
      portENTER_CRITICAL(&sharedDataMux);
      currentAvgTemp = sum / valid;
      portEXIT_CRITICAL(&sharedDataMux);
      
      // Вывод на дисплей
      disp.displayInt(round(currentAvgTemp));
    } else {
      disp.displayByte(_E, _r, _r, _empty);
    }

    // Управление реле
    portENTER_CRITICAL(&sharedDataMux);
    regulator.input = currentAvgTemp;
    regulator.setpoint = targetTemp;
    portEXIT_CRITICAL(&sharedDataMux);
    
    digitalWrite(RELAY_PIN, regulator.getResult());
  }
}
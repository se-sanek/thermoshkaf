#include <WiFi.h>
#include <WebServer.h>
#include <GyverRelay.h>
#include <GyverDS18Array.h>
#include <GyverSegment.h>

// ================= НАСТРОЙКИ СЕТИ =================
const char* ssid = "DTT-231/2";
const char* password = "#231-akv1";

// ================= ПИНЫ =================
#define CLK_PIN 2           // Дисплей
#define DIO_PIN 4           // Дисплей


WebServer server(80);

Disp1637Colon disp(DIO_PIN, CLK_PIN);

// установка, гистерезис, направление регулирования
GyverRelay reg1(REVERSE);
GyverRelay reg2(REVERSE);
GyverRelay reg3(REVERSE);
GyverRelay reg4(REVERSE);
// либо GyverRelay regulator(); без указания направления (будет REVERSE)

volatile float currentAvgTemp = 0.0;
volatile float targetTemp = 25.0; 

void display(float t);

uint64_t addr[] = {
  0x1F000000526CA628, // дно
  0xA300000051EF3628, // дверь
  0xAB0000005393F228, // середина
  0x730000007241EA28, // верх
};
GyverDS18Array ds(5, addr, 4);

static float tempIn[5];

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
  avg = tempIn[4];
  trg = targetTemp;
  portEXIT_CRITICAL(&sharedDataMux);

  html += "<div class='card'><h2>Средняя: " + String(avg, 1) + " C</h2>";
  html += "<p>Цель: <b>" + String(trg, 1) + "</b></p></div>";

  html += "<div class='card'><h3>Датчики</h3>";
  for(int i=0; i<4; i++) {
     html += "D" + String(i+1) + ": <b>" + String(tempIn[i], 1) + "</b> C<br>";
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

void setup() {
  Serial.begin(9600);
  ds.setResolution(12);  // для всех
  pinMode(18, OUTPUT);         // пин реле 1
  pinMode(19, OUTPUT);         // пин реле 2
  reg1.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg1.hysteresis = 2;   // ширина гистерезиса
  reg1.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg2.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg2.hysteresis = 2;   // ширина гистерезиса
  reg2.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg3.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg3.hysteresis = 2;   // ширина гистерезиса
  reg3.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg4.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg4.hysteresis = 2;   // ширина гистерезиса
  reg4.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  disp.printRight(true);

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
}

void loop() {
    // это таймер с периодом к setResolution, он сам делает request
  

  if (!ds.tick()) {
    float temps[4];
    ds.readTemps(temps);
    for (int i = 0; i < 4; i++) {
      tempIn[i] = temps[i];
    }
    tempIn[4] = 0;
    for(int i = 0; i < 4; i++){
    tempIn[4] += tempIn[i];
    }
    tempIn[4] /= 4;
    for(int i = 0; i < 4; i++){
      Serial.println(tempIn[i]);
    }
    Serial.println();
    Serial.println(tempIn[4]);
    Serial.println();
    Serial.println();
  }  
  reg1.input = tempIn[0];   // сообщаем регулятору текущую температуру
  reg2.input = tempIn[1];   // сообщаем регулятору текущую температуру
  reg3.input = tempIn[2];   // сообщаем регулятору текущую температуру
  reg4.input = tempIn[3];   // сообщаем регулятору текущую температуру

  // getResult возвращает значение для управляющего устройства
  digitalWrite(18, !(reg1.getResultTimer()||reg2.getResultTimer()||reg3.getResultTimer()||reg4.getResultTimer()));  // отправляем на реле (ОС работает по своему таймеру)
  digitalWrite(19, !reg4.getResultTimer());  // отправляем на реле (ОС работает по своему таймеру)
  display(tempIn[4]);
}

void display(float t){
  disp.clear();
  disp.print((int)round(t));
  disp.setCursor(3);
  disp.print('*');
  disp.update();
}

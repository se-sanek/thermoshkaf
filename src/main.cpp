#include <WiFi.h>
#include <WebServer.h>
#include <GyverRelay.h>
#include <GyverDS18Single.h>
#include <GyverDS18Array.h>
#include <GyverSegment.h>

// ================= НАСТРОЙКИ СЕТИ =================
const char* ssid = "DTT-231/2";
const char* password = "#231-akv1";

// ================= ПИНЫ =================
#define CLK_PIN 2           // Дисплей
#define DIO_PIN 4           // Дисплей


WebServer server(80);

Disp1637Colon disp(DIO_PIN, CLK_PIN, 1);

// установка, гистерезис, направление регулирования
GyverRelay reg1(REVERSE);
GyverRelay reg2(REVERSE);
GyverRelay reg3(REVERSE);
GyverRelay reg4(REVERSE);

volatile float currentAvgTemp = 0.0;
volatile float targetTemp = 25.0; 

void display(float t1, float t2, bool st1, bool st2);

uint64_t addr[] = {
  0x1F000000526CA628, // дно
  0xA300000051EF3628, // дверь
  0xAB0000005393F228, // середина
  0x730000007241EA28, // верх
  0x520000007CAAAF28, // дно
};
GyverDS18Array ds(5, addr, 5);
GyverDS18Single ds1(13);

static float tempIn[5];
static float tempOut;
bool relay1Stat;
bool relay2Stat;

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
  float avg, trg, out;
  bool s1, s2;
  portENTER_CRITICAL(&sharedDataMux);
  avg = tempIn[4];
  trg = targetTemp;
  out = tempOut;
  s1 = relay1Stat;
  s2 = relay2Stat;
  portEXIT_CRITICAL(&sharedDataMux);

  html += "<div class='card'><h3>Снаружи: " + String(out, 1) + " °C</h3>";
  html += "<h3>Средняя: " + String(avg, 1) + " °C</h3>";
  html += "<h3>Цель: <b>" + String(trg, 1) + " °C</b></h3></div>";

  html += "<div class='card'><h3>Датчики</h3>";
  html += "Внизу: <b>" + String(tempIn[0], 1) + "</b> °C<br>";
  html += "У двери: <b>" + String(tempIn[1], 1) + "</b> °C<br>";
  html += "В середине: <b>" + String(tempIn[2], 1) + "</b> °C<br>";
  html += "Вверху: <b>" + String(tempIn[3], 1) + "</b> °C<br>";
  html += "Нижний нагреватель: <b>";
  if(s1){
    html += "включен</b><br>";
  } else {
    html += "отключен</b><br>";
  }
  html += "Верхний нагреватель: <b>";
  if(s2){
    html += "включен</b><br>";
  } else {
    html += "отключен</b><br>";
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
    float temps[5];
    ds.readTemps(temps);
    for (int i = 0; i < 5; i++) {
      if(i < 4){
        tempIn[i] = temps[i];
      } else{
        tempOut = temps[i];
      }
    }
    tempIn[4] = 0;
    int val = 0;
    for(int i = 0; i < 4; i++){
      if(!isnan(tempIn[i])){
        tempIn[4] += tempIn[i];
        val++;
      }
    }
    tempIn[4] /= val;
    for(int i = 0; i < 4; i++){
      Serial.println(tempIn[i]);
    }
    Serial.println();
    Serial.println(tempIn[4]);
    Serial.println();
    Serial.println(tempOut);
    Serial.println();
    Serial.println();
  }  
  reg1.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg2.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg3.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg4.setpoint = targetTemp;    // установка (ставим на 40 градусов)
  reg1.input = tempIn[0];   // сообщаем регулятору текущую температуру
  reg2.input = tempIn[1];   // сообщаем регулятору текущую температуру
  reg3.input = tempIn[2];   // сообщаем регулятору текущую температуру
  reg4.input = tempIn[3];   // сообщаем регулятору текущую температуру

  // getResult возвращает значение для управляющего устройства
  relay1Stat = (reg1.getResultTimer()||reg2.getResultTimer()||reg3.getResultTimer()||reg4.getResultTimer());
  relay2Stat = reg4.getResultTimer();
  digitalWrite(18, !relay1Stat);  // отправляем на реле (ОС работает по своему таймеру)
  digitalWrite(19, !relay2Stat);  // отправляем на реле (ОС работает по своему таймеру)
  display(tempIn[4], tempOut, relay1Stat, relay2Stat);
}

void display(float t1, float t2, bool st1, bool st2){
  static uint32_t timer;
  static int flag = 0;
  disp.clear();
  if (millis() - timer >= 3000){
    flag++;
    if (flag > 2) flag = 0;
    if (flag == 2) {
      disp.colon(1);
    } else {
      disp.colon(0);
    }
    timer = millis();
  }
  switch (flag) {
  case 0:
    disp.print((int)round(t1));
    disp.setCursor(3);
    disp.print('*');
    break;
  
  case 1:
    disp.print((int)round(t2));
    disp.setCursor(3);
    disp.writeByte(0b01011100);
    //isp.print('>');
    break;
  
  case 2:
    if(st1){
      disp.writeByte(0b01011100);
      disp.print("N.");
    }else{
      disp.writeByte(0b01011100);
      disp.print("F.");
    }
    if(st2){
    disp.writeByte(0b01011100);
      disp.print("N.");
    }else{
    disp.writeByte(0b01011100);
      disp.print("F.");
    }
    break;
  }
  
  disp.update();
}

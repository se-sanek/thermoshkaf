#include <GyverRelay.h>
#include <GyverDS18Array.h>
#include <GyverSegment.h>

#define CLK_PIN 2
#define DIO_PIN 4

Disp1637Colon disp(DIO_PIN, CLK_PIN);

// установка, гистерезис, направление регулирования
GyverRelay reg1(REVERSE);
GyverRelay reg2(REVERSE);
GyverRelay reg3(REVERSE);
GyverRelay reg4(REVERSE);
// либо GyverRelay regulator(); без указания направления (будет REVERSE)

float tempReg = 27.0; 

void display(float t);

uint64_t addr[] = {
  0x1F000000526CA628, // дно
  0xA300000051EF3628, // дверь
  0xAB0000005393F228, // середина
  0x730000007241EA28, // верх
};
GyverDS18Array ds(5, addr, 4);

void setup() {
  Serial.begin(9600);
  ds.setResolution(12);  // для всех
  pinMode(18, OUTPUT);         // пин реле 1
  pinMode(19, OUTPUT);         // пин реле 2
  reg1.setpoint = tempReg;    // установка (ставим на 40 градусов)
  reg1.hysteresis = 2;   // ширина гистерезиса
  reg1.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg2.setpoint = tempReg;    // установка (ставим на 40 градусов)
  reg2.hysteresis = 2;   // ширина гистерезиса
  reg2.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg3.setpoint = tempReg;    // установка (ставим на 40 градусов)
  reg3.hysteresis = 2;   // ширина гистерезиса
  reg3.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  reg4.setpoint = tempReg;    // установка (ставим на 40 градусов)
  reg4.hysteresis = 2;   // ширина гистерезиса
  reg4.k = 0.5;          // коэффициент обратной связи (подбирается по факту)
  disp.printRight(true);
}

void loop() {
    // это таймер с периодом к setResolution, он сам делает request
  static float tempIn[5];

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
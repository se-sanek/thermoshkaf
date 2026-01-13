#include <GyverSegment.h>

#define CLK_PIN 4
#define DIO_PIN 2

Disp1637Colon disp(DIO_PIN, CLK_PIN);

// асинхронный опрос одного датчика на пине
// простейший вариант - библиотека сама запрашивает температуру
// и сигналит по готовности, можно забрать результат

#include <GyverDS18.h>

GyverDS18Single dso(12);  // пин

#include <GyverDS18Array.h>

uint64_t addr[] = {
    0x4D0417508099FF28, //датчик на дне
    0xFE04175159CDFF28, //датчик воздуха в центре
    0xC3041750E553FF28, //датчик наверху
    0xCF0417505B78FF28, //датчик у двери
};

GyverDS18Array dsi(5, addr, 4);

void setup() {
  dso.setResolution(9);
  dsi.setResolution(12);
  disp.printRight(true);
}
void loop() {
  static int tempOut;
  static float tempIn;
  if (!dso.tick()) {
    tempOut = dso.getTemp()*10;
  }
  float temps[4];
  if (!dsi.tick()) {
    dsi.readTemps(temps);
    //tempIn = 0;        
  }
  disp.clear();
  disp.print((int)temps[1]);
  disp.setCursor(3);
  disp.print('*');
  disp.update();
}

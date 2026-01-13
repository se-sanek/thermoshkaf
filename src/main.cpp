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
    0x4D0417508099FF28,
    0xFE04175159CDFF28,
    0xC3041750E553FF28,
    0xCF0417505B78FF28,
};
GyverDS18Array dsi(5, addr, 4);

void setup() {
  dso.setResolution(9);
  dsi.setResolution(12);
  disp.printRight(true);
}
void loop() {
  static int tempOut;
  static int tempIn;
    if (!dso.tick()) {
      tempOut = dso.getTemp()*10;
    }
    if (!dsi.tick()) {
        
        float temps[4];
        dsi.readTemps(temps);
        for (float t : temps) {
          tempIn = 0;
          tempIn += t;
        }
        tempIn *= 0.25;
    }
    disp.clear();
    disp.print(tempIn);
    disp.setCursor(3);
    disp.print('*');
    disp.update();
}
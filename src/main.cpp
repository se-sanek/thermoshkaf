#include <GyverSegment.h>

#define CLK_PIN 4
#define DIO_PIN 2

Disp1637Colon disp(DIO_PIN, CLK_PIN);

// асинхронный опрос одного датчика на пине
// простейший вариант - библиотека сама запрашивает температуру
// и сигналит по готовности, можно забрать результат

#include <GyverDS18.h>

GyverDS18Single ds(12);  // пин

void setup() {
    ds.setResolution(12);
  disp.printRight(true);
}
void loop() {
    if (!ds.tick()) {
      int temp1 = ds.getTemp()*10;
      disp.clear();
      disp.print(temp1);
      disp.setCursor(3);
      disp.print('*');
      disp.update();
    }
}
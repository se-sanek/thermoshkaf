#ifndef DISPLAY_LOGIC_H
#define DISPLAY_LOGIC_H

#include "Settings.h"

void updateDisplay() {
    disp.printRight(true);
    static uint32_t tmr;
    static byte mode = 0;
    
    // Обновляем данные на дисплее раз в 2 секунды
    if (millis() - tmr < 2000) return;
    tmr = millis();
    
    disp.clear();
    
    if (mode == 0) {
        // Выводим внутреннюю температуру
        int val = (int)round(tempIn[4]);
        
        // Чтобы число не затиралось символом на 4-м разряде:
        //if (val > 99) val = 99; // Ограничение для красоты
        
        disp.setCursor(3);    // Начинаем с самого первого разряда
        disp.print(val);      // Печатаем число
        
        //disp.setCursor(3);    // А символ градуса ставим строго в конец
        disp.print('*'); 
        
        mode = 1;
    } else {
        // Выводим уличную температуру
        int val = (int)round(tempOut);
        disp.setCursor(3);
        //disp.setCursor(0);
        disp.print(val);
        
        //disp.setCursor(3);
        disp.writeByte(0b01011100); // Символ 'u' (улица)
        
        mode = 0;
    }
    
    disp.update();
}

#endif
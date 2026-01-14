#include <Arduino.h>
#include <GyverDS18.h>
#define DS_PIN 2

void setup() {
  Serial.begin(9600);
    
  uint64_t addr[5] = {};
  uint8_t res = gds::Search(DS_PIN).scan(addr, 5);
  Serial.println(res);  // кол-во найденных

  // выведем в порт
  for (uint64_t& a : addr) {
    gds::Addr(a).printTo(Serial);
  }
    
}


void loop() {
}

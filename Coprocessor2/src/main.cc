#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

void 
setup() {
    Wire.begin();
    SPI.begin();
    EEPROM.begin();
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial2.begin(115200);
    Serial3.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
}

void 
loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
}
void
yield() {
}
void
serialEventRun() {

}
void
serialEventRun1() {

}
void
serialEventRun2() {

}
void
serialEventRun3() {

}

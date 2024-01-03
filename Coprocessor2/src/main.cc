#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
void intVect0();
void intVect1();
void 
setup() {
    Serial.begin(250000);
    Serial.println(F("COPROCESSOR 2 STARTUP"));
    Wire.begin();
    Serial.println(F("WIRE STARTED"));
    SPI.begin();
    Serial.println(F("SPI STARTED"));
    EEPROM.begin();
    Serial.println(F("EEPROM STARTED"));
    Serial1.begin(250000);
    Serial.println(F("Serial1 STARTED"));
    Serial2.begin(250000);
    Serial.println(F("Serial2 STARTED"));
    Serial3.begin(250000);
    Serial.println(F("Serial3 STARTED"));
    Serial.println(F("Configuring Pins"));
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(F("Attaching interrupts"));
    attachInterrupt(digitalPinToInterrupt(2), intVect0, FALLING);
    attachInterrupt(digitalPinToInterrupt(3), intVect1, FALLING);
    Serial.println(F("Booted!"));
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
serialEvent() {

}
void
serialEvent1() {

}
void
serialEvent2() {

}
void
serialEvent3() {

}

void
intVect0() {

}

void
intVect1() {

}

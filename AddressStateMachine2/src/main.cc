#include <Arduino.h>

constexpr auto ADSPin = 4;
constexpr auto BLASTPin = 5;
constexpr auto READYPin = 6;
constexpr auto AddressStatePin = 7;
constexpr auto DataStatePin = 8;
void setup() {
    pinMode(ADSPin, INPUT);
    pinMode(BLASTPin, INPUT);
    pinMode(READYPin, INPUT);
    pinMode(AddressStatePin, OUTPUT);
    pinMode(DataStatePin, OUTPUT);
    digitalWrite(AddressStatePin, HIGH);
    digitalWrite(DataStatePin, HIGH);
}

void loop() {
    while (digitalRead(ADSPin) == HIGH);
    digitalWrite(AddressStatePin, LOW);
    while (digitalRead(ADSPin) == LOW);
    digitalWrite(AddressStatePin, HIGH);
    digitalWrite(DataStatePin, LOW);
    while (digitalRead(BLASTPin) == HIGH);
    while (digitalRead(BLASTPin) == LOW);
    digitalWrite(DataStatePin, HIGH);
}

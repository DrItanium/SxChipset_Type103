#include <Arduino.h>

constexpr auto Reset960Pin = 7;
void setup() {
    pinMode(Reset960Pin, OUTPUT);
    digitalWrite(Reset960Pin, LOW);
    Serial.begin(9600);
    while(!Serial);
    Serial1.begin(115200);
    digitalWrite(Reset960Pin, HIGH);
}

void loop() {
    if (Serial.available()) {
        Serial1.write(Serial.read());
    }
    if (Serial1.available()) {
        Serial.write(Serial1.read());
    }
}



#include <Arduino.h>

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    Serial.begin(9600);
    while(!Serial);
    Serial.println("Startup");
    Serial1.begin(115200);
}

void loop() {
    while (Serial1.available()) {
        Serial.write(Serial1.read());
    }
}

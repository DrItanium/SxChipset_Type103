#include <Arduino.h>

void setup() {
    Serial.begin(9600);
    while(!Serial);
    Serial.println("Startup");
}

void loop() {
    Serial.println("Boop");
    delay(1000);
}

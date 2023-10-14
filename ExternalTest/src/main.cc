#include <Arduino.h>
constexpr auto Reset960Pin = 7;
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(Reset960Pin, OUTPUT);
    digitalWrite(Reset960Pin, LOW);
    Serial.begin(9600);
    while(!Serial);
    Serial1.begin(115200);

    digitalWrite(Reset960Pin, HIGH);
}

void loop() {
    while (Serial1.available()) {
        Serial.write(Serial1.read());
    }
}

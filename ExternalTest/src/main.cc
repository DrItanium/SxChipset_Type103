#include <Arduino.h>
#include <PacketSerial.h>

constexpr auto Reset960Pin = 7;
PacketSerial packetManager_Serial2;
void onPacketReceived(const PacketSerial& sender, const uint8_t* buffer, size_t size) {

}
void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    pinMode(Reset960Pin, OUTPUT);
    digitalWrite(Reset960Pin, LOW);
    Serial.begin(9600);
    while(!Serial);
    Serial1.begin(115200);
    Serial2.begin(115200);
    packetManager_Serial2.setStream(&Serial2);
    packetManager_Serial2.setPacketHandler([](const uint8_t* buffer, size_t size) { onPacketReceived(packetManager_Serial2, buffer, size); });
    digitalWrite(Reset960Pin, HIGH);
}

void loop() {
    if (Serial.available()) {
        Serial1.write(Serial.read());
    }
    if (Serial1.available()) {
        Serial.write(Serial1.read());
    }
    packetManager_Serial2.update();

    if (packetManager_Serial2.overflow()) {
        digitalWrite(LED_BUILTIN, HIGH);
    } else {
        digitalWrite(LED_BUILTIN, LOW);
    }
}



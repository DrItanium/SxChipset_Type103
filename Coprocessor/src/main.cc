/**
* i960SxChipset_Type103
* Copyright (c) 2022-2023, Joshua Scoggins
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#define DEFAULT_CS PIN_PF2
#define SD_PIN PIN_PF3
#define DAZZLER_SEL PIN_PE3
#include <GD2.h>
template<typename T>
void printCharactersToSerial1(T& source) noexcept {
    while (source.available()) {
        auto theChar = static_cast<char>(source.read());
        switch (theChar) {
            case '\n':
            case '\r':
                Serial1.print("\r\n");
                break;
            default:
                Serial1.print(theChar);
                break;
        }
    }
}
enum class Opcodes : byte {
    SerialConsole,
};
void
discardBytes() noexcept {
    while (Wire.available() > 0) {
        (void)Wire.read();
    }
}
void
printToSerial() noexcept {
    printCharactersToSerial1(Wire);
}
void 
onReceive(int) noexcept {
    switch (static_cast<Opcodes>(Wire.read())) {
        case Opcodes::SerialConsole:
            printToSerial();
            break;
        default:
            discardBytes();
            break;
    }
}
void onRequest() noexcept {

}


void setup() {
    pinMode(DEFAULT_CS, OUTPUT);
    pinMode(SD_PIN, OUTPUT);
    pinMode(DAZZLER_SEL, OUTPUT);
    digitalWrite(DEFAULT_CS, HIGH);
    digitalWrite(SD_PIN, HIGH);
    digitalWrite(DAZZLER_SEL, HIGH);
    Serial.begin(115200);
    Serial1.swap(1);
    Serial1.begin(115200);
    SPI.swap(2);
    SPI.begin();
    digitalWrite(DAZZLER_SEL, LOW);
    SPI.transfer(0x41);
    SPI.transfer(0x01);
    digitalWrite(DAZZLER_SEL, HIGH);
    Wire.begin(8);
    Wire.onReceive(onReceive);
}

void loop() {
    // only send data off to the dazzler in text mode, we have to act as a
    // passthrough
    printCharactersToSerial1(Serial);
}

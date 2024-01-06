/*
i960SxChipset_Type103
Copyright (c) 2022, Joshua Scoggins
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>

uint8_t* memoryBegin = nullptr;
uint8_t* memoryEnd = nullptr;
constexpr auto ExternalMemoryBaseAddress = 0x7000'0000;
constexpr auto WireAddress = 8;
size_t memorySizeInBytes = 0;
extern "C" uint8_t external_psram_size;
void
handleWireEvent(int howMany) {

}
void 
setup() {
    Serial.begin(115'200);
    while (!Serial);
    Serial8.begin(500'000);
    while (!Serial8);
    Wire.begin(WireAddress);
    Wire.onReceive(handleWireEvent);
    if (SD.begin(BUILTIN_SDCARD)) {
        Serial.println(F("SD CARD is connected!"));
    } else {
        Serial.println(F("SD CARD is not found!"));
    }
    Serial.printf(F("EXTMEM Memory Test Size: %d Mbyte\n"), external_psram_size);
    if (external_psram_size > 0) {
        memoryBegin = reinterpret_cast<uint8_t*>(ExternalMemoryBaseAddress);
        memorySizeInBytes = (external_psram_size * (1024 * 1024));
        memoryEnd = reinterpret_cast<uint8_t*>(ExternalMemoryBaseAddress + memorySizeInBytes);
        Serial.println(F("External Memory Available"));
        for (size_t i = 0; i < memorySizeInBytes; ++i) {
            memoryBegin[i] = 0;
        }
        Serial.println(F("External Memory Cleared!"));
    } else {
        Serial.println(F("External Memory Not Available"));
    }
}



void 
loop() {

}

void
serialEvent8() {

}

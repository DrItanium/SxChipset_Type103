/*
i960SxChipset_Type103
Copyright (c) 2022-2023, Joshua Scoggins
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
constexpr size_t IBUSFirstAddress = 0x4000;
constexpr size_t IBUSLastAddress = 0x7FFF;
volatile byte* IBUSEndAddress = reinterpret_cast<volatile byte*>(IBUSLastAddress + 1);
void 
setIBUSBank(uint8_t bankId) noexcept {
    PORTJ = bankId;
}
void
setup() {
    randomSeed(analogRead(A0) + analogRead(A1) + analogRead(A2) + analogRead(A3));
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    DDRJ = 0xFF; // output
    setIBUSBank(0);
    // enable EBI
    XMCRB = 0b00000'000; // full 64k space
    XMCRA = 0b1'000'11'11; // no wait states and enable EBI
    Serial.println(F("Testing the first 16k of the IBUS"));
    for (volatile byte* window = reinterpret_cast<volatile byte*>(IBUSFirstAddress); window < IBUSEndAddress; ++window) {
        volatile byte theValue = static_cast<uint8_t>(random());
        *window = theValue;
        if (*window != theValue) {
            Serial.print(F("Mismatch @0x"));
            Serial.print(reinterpret_cast<uintptr_t>(window), HEX);
            Serial.print(F(", G: 0x"));
            Serial.print(*window, HEX);
            Serial.print(F(", W: 0x"));
            Serial.print(theValue, HEX);
            Serial.println();
        }
    }
    Serial.println(F("DONE!"));
}


void 
loop() {
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
}


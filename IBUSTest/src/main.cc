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
bankTest() noexcept {
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
            Serial.println(F("\tSkipping the rest of the bank"));
            return;
        }
    }
}

void
IBUSTest() noexcept {
    Serial.println(F("Testing the first 16k of the IBUS"));
    for (uint16_t bankIndex = 0; bankIndex < 0x100; ++bankIndex) {
        Serial.print(F("Setting to bank 0b"));
        Serial.println(bankIndex, BIN);
        setIBUSBank(bankIndex); 
        bankTest();
    }
    Serial.println(F("DONE!"));
}
template<typename T>
volatile T& memory(uintptr_t address) noexcept {
    return *reinterpret_cast<volatile T*>(address);
}

union [[gnu::packed]] CH351 {
    static constexpr auto DirectionOutput = 1;
    static constexpr auto DirectionInput = 0;
    uint8_t registers[8];
    struct {
        uint8_t gpio[4];
        uint8_t direction[4];
    } reg8;
    struct {
        uint32_t gpio;
        uint32_t direction;
    } reg32;
    struct {
        uint16_t gpio[2];
        uint16_t direction[2];
    } reg16;
    static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;
    struct {
        uint8_t hold : 1;
        uint8_t hlda : 1;
        uint8_t lock : 1;
        uint8_t fail : 1;
        uint8_t reset : 1;
        uint8_t cfg : 3;
        uint8_t freq : 3;
    } controlSignals;
};
void
CH351DevicesTest() noexcept {
    Serial.println(F("Testing CH351 devices!"));
    volatile CH351& addressLines = memory<CH351>(0x2200);
    volatile CH351& dataLines = memory<CH351>(0x2208);
    volatile CH351& controlSignals = memory<CH351>(0x2210);
    volatile CH351& bankRegister = memory<CH351>(0x2218);
    addressLines.reg32.direction = 0;
    dataLines.reg32.direction = 0xFFFF'FFFF;
    controlSignals.reg32.direction = CH351::ControlSignalDirection;
    bankRegister.reg32.direction = 0xFFFF'FFFF;
    Serial.println(F("DONE!"));
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
    XMCRB = 0b1'0000'000; // full 64k space and bus keeper
    XMCRA = 0b1'000'01'01; // need a single cycle wait state and enable EBI
    IBUSTest();
    CH351DevicesTest();
}


void 
loop() {
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
}


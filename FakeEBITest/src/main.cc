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
#include <SPI.h>
#include <Wire.h>


#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;

constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;
constexpr bool PerformMemoryImageInstallation = true;
constexpr uintptr_t MemoryWindowBaseAddress = 0x4000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;






[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2208)]] volatile uint8_t dataLines[4];
[[gnu::address(0x2208)]] volatile uint32_t dataLinesFull;
[[gnu::address(0x2208)]] volatile uint16_t dataLinesHalves[2];
[[gnu::address(0x220C)]] volatile uint32_t dataLinesDirection;
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_bytes[4];
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_LSB;

[[gnu::address(0x2200)]] volatile uint16_t AddressLines16Ptr[4];
[[gnu::address(0x2200)]] volatile uint32_t AddressLines32Ptr[2];
[[gnu::address(0x2200)]] volatile uint32_t addressLinesValue32;
[[gnu::address(0x2200)]] volatile uint16_t addressLinesLowerHalf;
[[gnu::address(0x2200)]] volatile uint8_t addressLines[8];
[[gnu::address(0x2200)]] volatile uint8_t addressLinesLowest;
[[gnu::address(0x2200)]] volatile uint24_t addressLinesLower24;
[[gnu::address(0x2204)]] volatile uint32_t addressLinesDirection;



void
setupExternalBus() noexcept {
    // setup the EBI
    XMCRB=0b1'0000'000;
    XMCRA=0b1'010'01'01;  
    // the CH351 does not return the direction register contents on a read
    AddressLinesInterface.view32.direction = 0xFFFF'FFFE;
    AddressLinesInterface.view32.data = 0;
    DataLinesInterface.view32.direction = 0;
}

uint32_t readFromBus(uint32_t address) noexcept {
    AddressLinesInterface.view32.data = address;
    DataLinesInterface.view32.direction = 0;
    digitalWrite<Pin::BE0, LOW>();
    digitalWrite<Pin::BE1, LOW>();
    digitalWrite<Pin::BE2, LOW>();
    digitalWrite<Pin::BE3, LOW>();
    digitalWrite<Pin::WR, LOW>();
    digitalWrite<Pin::DEN, LOW>();
    uint32_t result = DataLinesInterface.view32.data;
    digitalWrite<Pin::DEN, HIGH>();
    return result;
}

void writeToBus(uint32_t address, uint32_t value) noexcept {
    AddressLinesInterface.view32.data = address;
    digitalWrite<Pin::BE0, LOW>();
    digitalWrite<Pin::BE1, LOW>();
    digitalWrite<Pin::BE2, LOW>();
    digitalWrite<Pin::BE3, LOW>();
    digitalWrite<Pin::WR, HIGH>();
    digitalWrite<Pin::DEN, LOW>();
    DataLinesInterface.view32.direction = 0xFFFF'FFFF;
    DataLinesInterface.view32.data = value;
    asm volatile ("nop");
    asm volatile ("nop");
    digitalWrite<Pin::DEN, HIGH>();
}


void
setup() {
    Serial.begin(115200);
    pinMode(Pin::BE0, OUTPUT);
    pinMode(Pin::BE1, OUTPUT);
    pinMode(Pin::BE2, OUTPUT);
    pinMode(Pin::BE3, OUTPUT);
    pinMode(Pin::DEN, OUTPUT);
    pinMode(Pin::WR, OUTPUT);
    digitalWrite<Pin::WR, HIGH>();
    digitalWrite<Pin::BE0, LOW>();
    digitalWrite<Pin::BE1, LOW>();
    digitalWrite<Pin::BE2, LOW>();
    digitalWrite<Pin::BE3, LOW>();
    digitalWrite<Pin::DEN, HIGH>();
    setupExternalBus();
}
volatile uint24_t flashAddress = 0;
constexpr uint32_t sramBaseAddress = 16ul * 1024ul * 1024ul;
void 
loop() {
    Serial.print(F("Read from 0x"));
    Serial.print(static_cast<uint32_t>(flashAddress), HEX);
    Serial.print(F(": 0x"));
    auto result = readFromBus(flashAddress);
    Serial.println(result, HEX);
    Serial.print(F("Write to 0x"));
    uint32_t dest = flashAddress + sramBaseAddress;
    Serial.print(dest, HEX);
    Serial.print(F(" the value 0x"));
    Serial.println(result, HEX);
    writeToBus(dest, result);

    Serial.print(F("Read from 0x"));
    Serial.print(dest, HEX);
    Serial.print(F(": 0x"));
    auto result2 = readFromBus(dest);
    Serial.println(result2, HEX);
    if (result != result2) {
        Serial.println(F("MISMATCH!!!!"));
    }
    flashAddress+=4;
}


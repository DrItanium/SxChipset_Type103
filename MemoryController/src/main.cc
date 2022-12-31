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
#include <SD.h>
constexpr auto PSRAMEnable = 2;
constexpr auto SDPin = 10;
constexpr auto BANK0 = 45;
constexpr auto BANK1 = 44;
constexpr auto BANK2 = 43;
constexpr auto BANK3 = 42;
using Ordinal = uint32_t;
using LongOrdinal = uint64_t;

union SplitWord32 {
    constexpr SplitWord32 (Ordinal a) : whole(a) { }
    Ordinal whole;
    struct {
        Ordinal lower : 15;
        Ordinal a15 : 1;
        Ordinal a16_23 : 8;
        Ordinal a24_31 : 8;
    } splitAddress;
    struct {
        Ordinal lower : 15;
        Ordinal bank0 : 1;
        Ordinal bank1 : 1;
        Ordinal bank2 : 1;
        Ordinal bank3 : 1;
        Ordinal rest : 13;
    } internalBankAddress;
    struct {
        Ordinal offest : 28;
        Ordinal space : 4;
    } addressKind;
    [[nodiscard]] constexpr bool inIOSpace() const noexcept { return addressKind.space == 0b1111; }
};
union SplitWord64 {
    LongOrdinal whole;
    Ordinal parts[sizeof(LongOrdinal) / sizeof(Ordinal)];
};
void set328BusAddress(const SplitWord32& address) noexcept;
void setInternalBusAddress(const SplitWord32& address) noexcept;
#if 0
void setup() {
    Serial.begin(115200);
    SPI.begin();
    enableXMEM();
    Wire.begin(8);
    Wire.onReceive(onReceive);
}
#endif

void
onReceive(int howMany) noexcept {

}

void
onRequest() noexcept {

}


void
setupTWI() noexcept {
    Serial.print(F("Configuring TWI..."));
    Wire.begin(8);
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.println(F("DONE"));
}
void
setupSPI() noexcept {
    Serial.print(F("Configuring SPI..."));
    SPI.begin();
    Serial.println(F("DONE"));
}
void
setupSerial(bool displayBootScreen = true) noexcept {
    Serial.begin(115200);
    if (displayBootScreen) {
        Serial.println(F("i960 Simulator (C) 2022 and beyond Joshua Scoggins"));
        Serial.println(F("This software is open source software"));
        Serial.println(F("Base Platform: Arduino Mega2560"));
    }
}

void 
setup() {
    setupSerial();
    Serial.println(F("Bringing up peripherals"));
    Serial.println();
    setupSPI();
    setupTWI();
    Serial.print(F("Configuring GPIOs..."));
    pinMode(BANK0, OUTPUT);
    pinMode(BANK1, OUTPUT);
    pinMode(BANK2, OUTPUT);
    pinMode(BANK3, OUTPUT);
    pinMode(SDPin, OUTPUT);
    pinMode(PSRAMEnable, OUTPUT);
    digitalWrite(SDPin, HIGH);
    digitalWrite(PSRAMEnable, HIGH);

    Serial.println(F("DONE"));
    Serial.print(F("Setting up EBI..."));
    // cleave the address space in half via sector limits.
    // lower half is io space for the implementation
    // upper half is the window into the 32/8 bus
    XMCRB = 0;           // No external memory bus keeper and full 64k address
                         // space
    XMCRA = 0b1100'0000; // Divide the 64k address space in half at 0x8000, no
                         // wait states activated either. Also turn on the EBI
    set328BusAddress(0);
    setInternalBusAddress(0);
    Serial.println(F("DONE"));

    Serial.println(F("BOOT COMPLETE!!"));
}
void 
loop() {
}

void
set328BusAddress(const SplitWord32& address) noexcept {
    // set the upper
    PORTF = address.splitAddress.a24_31;
    PORTK = address.splitAddress.a16_23;
    digitalWrite(38, address.splitAddress.a15);
}

void
setInternalBusAddress(const SplitWord32& address) noexcept {
    digitalWrite(BANK0, address.internalBankAddress.bank0);
    digitalWrite(BANK1, address.internalBankAddress.bank1);
    digitalWrite(BANK2, address.internalBankAddress.bank2);
    digitalWrite(BANK3, address.internalBankAddress.bank3);
}

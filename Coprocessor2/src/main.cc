/*
i960SxChipset_Type103
Copyright (c) 2022-2024, Joshua Scoggins
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
#include <Wire.h>
#include <SPI.h>
auto& Console = Serial1;
auto& CommunicationChannel0 = Serial2;
auto& CommunicationChannel1 = Serial;
void
setupClockSource() noexcept {
    // enable 20MHz clock to be emitted on PA7
    CCP = 0xD8;
    CLKCTRL.MCLKCTRLA = 0b1000'0000;
    CCP = 0xD8;
    // make sure that the 20MHz out runs even when we are in standby
    CCP = 0xD8;
    CLKCTRL.OSC20MCTRLA |= 0b0000'0010;
    CCP = 0xD8;
}
void 
setup() {
    setupClockSource();
    delay(2000);
    // setup the pins
    pinConfigure(PIN_PA0, PIN_DIR_OUTPUT, PIN_OUT_LOW);
    Console.swap(1);
    CommunicationChannel0.swap(1);
    Console.begin(115200);
    CommunicationChannel0.begin(115200);
    CommunicationChannel1.begin(115200);
}



void 
loop() {

}

void
serialEvent2() {
    // communication channel 1
    while (CommunicationChannel0.available()) {
        auto value = CommunicationChannel0.read();
    }
}

void
serialEvent() {
    // communication channel 2
    while (CommunicationChannel1.available()) {
        auto value = CommunicationChannel1.read();
    }
}

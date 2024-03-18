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
#include <PacketSerial.h>
auto& Console = Serial1;
auto& ChannelTo2560 = Serial2;
constexpr auto LedPin = LED_BUILTIN;
void
setupClockSource() noexcept {
    // take in the 20MHz signal from the board but do not output a signal
    CCP = 0xD8;
    CLKCTRL.MCLKCTRLA = 0b0000'0011;
    CCP = 0xD8;
}
void 
setup() {
    setupClockSource();
    // setup the pins
    Console.swap(1);
    ChannelTo2560.swap(1);
    Console.begin(115200);
    ChannelTo2560.begin(115200);
    Console.println("System Up");
    pinConfigure(LedPin, PIN_DIR_OUTPUT, PIN_OUT_LOW);
    // setup the other communication channels
}



void 
loop() {
    digitalWriteFast(LedPin, HIGH);
    delay(1000);
    digitalWriteFast(LedPin, LOW);
    delay(1000);
}

void
serialEvent2() {

}

void
serialEvent1() {

}

void
serialEvent() {

}

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
#define Console Serial1
#define ChannelTo2560 Serial2
constexpr auto LedPin = LED_BUILTIN;

#define DataPortI960_Lower VPORTD
// CCL hookups
// this CCL is responsible for controlling the data lines transceiver output enable
constexpr auto Signal_Chip_Enable = PIN_PC0;
constexpr auto Signal_Data_Enable = PIN_PC1;
constexpr auto Signal_ByteEnable = PIN_PC2;
constexpr auto Signal_Transceiver_Enable = PIN_PC3;
// this CCL is responsible for controlling the data lines transceiver direction
constexpr auto Signal_WR = PIN_PF0;
constexpr auto Signal_DTR = PIN_PF1;
constexpr auto Signal_Transceiver_Direction = PIN_PF3;
// this pin is responsible for starting the data transaction
constexpr auto Signal_ADS = PIN_PE0;
// this pin is responsible for signalling ready to the 2560 or the i960, it
// uses the alternate output for the CCL connected to PortA
constexpr auto Signal_READY = PIN_PA6;
[[gnu::always_inline]]
inline
uint8_t 
readDataLinesLower() noexcept {
    return DataPortI960_Lower.IN;
}
[[gnu::always_inline]]
inline
void
writeDataLinesLower(uint8_t value) noexcept {
    DataPortI960_Lower.OUT = value;
}

[[gnu::always_inline]]
inline
void 
setDataLinesLowerDirection(uint8_t value) noexcept {
    DataPortI960_Lower.DIR = value;
}

void
setupClockSource() noexcept {
    // take in the 20MHz signal from the board but do not output a signal
    CCP = 0xD8;
    CLKCTRL.MCLKCTRLA = 0b0000'0011;
    CCP = 0xD8;
}
void
configureGPIO() {
    setDataLinesLowerDirection(0x00);
}
void 
setup() {
    setupClockSource();
    configureGPIO();
    // setup the pins
    Console.swap(1);
    ChannelTo2560.swap(1);
    Console.begin(115200);
    ChannelTo2560.begin(115200);
    Console.println("System Up");
    // setup the other communication channels
}



void 
loop() {
}


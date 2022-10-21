/*
i960SxChipset_Type103
Copyright (c) 2022-2021, Joshua Scoggins
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
#include <SdFat.h>
SdFat SD;
enum class Pinout : byte {
#define DefPin(port, index) Port ## port ## index 
#define DefPort(port) \
    DefPin(port, 0), \
    DefPin(port, 1), \
    DefPin(port, 2), \
    DefPin(port, 3), \
    DefPin(port, 4), \
    DefPin(port, 5), \
    DefPin(port, 6), \
    DefPin(port, 7)
DefPort(B),
DefPort(D),
DefPort(C),
DefPort(A),
Count,
#undef DefPin
#undef DefPort
// concepts
DEN,
W_R_,
FAIL,
Ready,
SD_EN,
GPIOSelect,
SPI1_EN3,
ADDR_INT0,
ADDR_INT1,
ADDR_INT2,
ADDR_INT3,
DATA_INT0,
DATA_INT1,
XIO_INT,
RAM_IO,
#define X(ind) SPI2_EN ## ind 
X(0),
X(1),
X(2),
X(3),
X(4),
X(5),
X(6),
X(7),
#undef X
    HOLD = PortB0,
    CLKO = PortB1,
    HLDA = PortB2,
    CS2 = PortB3,
    CS1 = PortB4,

    INT0_ = PortD5,
    SEL = PortD6,
    INT3_ = PortD7,

    SPI_OFFSET0 = PortC2,
    SPI_OFFSET1 = PortC3,
    SPI_OFFSET2 = PortC4,
    SPI2_OFFSET0 = PortC5,
    SPI2_OFFSET1 = PortC6,
    SPI2_OFFSET2 = PortC7,
    Capture0 = PortA0,
    Capture1 = PortA1,
    Capture2 = PortA2,
    Capture3 = PortA3,
    Capture4 = PortA4,
    Capture5 = PortA5,
    Capture6 = PortA6,
    Capture7 = PortA7,
};
enum class Port : byte {
    A,
    B,
    C,
    D,
    None,
};
using PortOutputRegister = volatile byte&;
using PortInputRegister = volatile byte&;
using PortDirectionRegister = volatile byte&;
constexpr Port getPort(Pinout pin) noexcept {
    switch (pin) {
#define X(port, ind) case Pinout :: Port ## port ## ind : return Port:: port 
#define Y(port) \
        X(port, 0); \
        X(port, 1); \
        X(port, 2); \
        X(port, 3); \
        X(port, 4); \
        X(port, 5); \
        X(port, 6); \
        X(port, 7)
        Y(A);
        Y(B);
        Y(C);
        Y(D);
#undef Y
#undef X
        default: return Port::None;
    }
}
constexpr bool validPort(Port port) noexcept {
    switch (port) {
        case Port::A:
        case Port::B:
        case Port::C:
        case Port::D:
            return true;
        default:
            return false;
    }
}
constexpr bool isPhysicalPin(Pinout pin) noexcept {
    return validPort(getPort(pin));
}

template<Pinout pin>
constexpr auto IsPhysicalPin_v = isPhysicalPin(pin);

PortOutputRegister 
getOutputRegister(Port port) noexcept {
    switch (port) {
        case Port::A: return PORTA;
        case Port::B: return PORTB;
        case Port::C: return PORTC;
        case Port::D: return PORTD;
        default: return GPIOR0;
    }
}
[[gnu::always_inline]] 
inline void 
digitalWrite(Pinout pin, decltype(LOW) value) noexcept { 
    digitalWrite(static_cast<byte>(pin), value); 
}

[[gnu::always_inline]] 
inline decltype(LOW)
digitalRead(Pinout pin) noexcept { 
    return digitalRead(static_cast<byte>(pin));
}
class CacheEntry {

};

void 
setup() {
    Serial.begin(115200);
    SPI.begin();
    while (!SD.begin()) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
}

void 
loop() {
    delay(1000);
}

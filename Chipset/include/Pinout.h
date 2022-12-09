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

#ifndef SXCHIPSET_TYPE103_PINOUT_H
#define SXCHIPSET_TYPE103_PINOUT_H
#include <Arduino.h>
enum class Pin : byte {
#define X(port, index) Port ## port ## index = PIN_P ## port ## index,
#include "AVRPins.def"
#ifdef NUM_DIGITAL_PINS
Count = NUM_DIGITAL_PINS,
#else
#error "Count must equal the number of digital pins available"
#endif
#undef X
    // fake entries
    XIO2,
    XIO3,
    XIO4,
    XIO5,
    XIO6,
    XIO7,
    // aliases
    SEL = PortB0,
    CLKO = PortB1,
    Ready = PortB2,
    SEL1 = PortB3,
    GPIOSelect = PortB4,
    INT0_ = PortD7,

    Capture0 = PortA0,
    Capture1 = PortA1,
    Capture2 = PortA2,
    Capture3 = PortA3,
    Capture4 = PortA4,
    Capture5 = PortA5,
    Capture6 = PortA6,
    Capture7 = PortA7,
    // channel 0
    BE0 = Capture0,
    BE1 = Capture1,
    BLAST_ = Capture2,
    DEN = Capture3, 
    FAIL = Capture4, 
    DATA_INT0 = Capture5,
    DATA_INT1 = Capture6,
    ADDR_INT0 = Capture7,
#define X(x, y) ADR ## x = Capture ## y
    // channel 1
    X(16, 0),
    X(17, 1),
    X(18, 2),
    X(19, 3),
    X(20, 4),
    X(21, 5),
    X(22, 6),
    X(23, 7),
    // channel 2
    W_R_ = Capture0,
    X(1, 1),
    X(2, 2),
    X(3, 3),
    X(4, 4),
    X(5, 5),
    X(6, 6),
    X(7, 7),
    // channel 3
    X(8, 0),
    X(9, 1),
    X(10, 2),
    X(11, 3),
    X(12, 4),
    X(13, 5),
    X(14, 6),
    X(15, 7),
#undef X
    ShieldD0 = PortD0,
    ShieldD1 = PortD1,
    ShieldD2 = PortD2,
    ShieldD3 = PortD3,
    ShieldD4 = PortD4,
    ShieldD5 = PortD5,
    ShieldD6 = PortD6,
    ShieldD7 = PortC2,
    ShieldD8 = PortC3,
    ShieldD9 = PortC4,
    ShieldD10 = PortC5,
    ShieldD11 = PortB5,
    ShieldD12 = PortB6,
    ShieldD13 = PortB7,

    ShieldMOSI = ShieldD11,
    ShieldMISO = ShieldD12,
    ShieldSCK = ShieldD13,

    ShieldA0 = PortC6,
    ShieldA1 = PortC7,
    ShieldA2 = XIO7,
    ShieldA3 = GPIOSelect,
    ShieldA4 = PortC1,
    ShieldA5 = PortC0,
    PSRAM0 = ShieldD2,
    SD_EN = ShieldD10,
};
enum class Port : byte {
    // stop at mega2560 tier
#define X(name) name ,
#include "AVRPorts.def"
#undef X
    None,
};
using PortOutputRegister = volatile byte&;
using PortInputRegister = volatile byte&;
using PortDirectionRegister = volatile byte&;
#ifndef GPIOR0
extern byte GPIOR0;
#endif
#ifndef GPIOR1
extern byte GPIOR1;
#endif
#ifndef GPIOR2
extern byte GPIOR2;
#endif
constexpr Port getPort(Pin pin) noexcept {
    switch (pin) {
#define X(port, ind) case Pin :: Port ## port ## ind : return Port:: port ;
#include "AVRPins.def"
#undef X
        default: return Port::None;
    }
}
constexpr bool validPort(Port port) noexcept {
    switch (port) {
#define X(name) case Port :: name :
#include "AVRPorts.def"
#undef X
            return true;
        default:
            return false;
    }
}
[[gnu::always_inline]]
[[nodiscard]] constexpr decltype(auto) getPinMask(Pin pin) noexcept {
    switch (pin) {
#define X(port, offset) case Pin :: Port ## port ## offset : return _BV ( P ## port ## offset) ;
#include "AVRPins.def"
#undef X
        default:
            return 0xFF;
    }
}
template<Pin pin>
[[gnu::always_inline]]
[[nodiscard]] constexpr decltype(auto) getPinMask() noexcept {
    return getPinMask(pin);
}
constexpr bool isPhysicalPin(Pin pin) noexcept {
    return validPort(getPort(pin));
}

template<Pin pin>
constexpr auto IsPhysicalPin_v = isPhysicalPin(pin);
[[gnu::always_inline]]
[[nodiscard]] 
inline PortOutputRegister 
getOutputRegister(Port port) noexcept {
    switch (port) {
#define X(name) case Port :: name : return PORT ## name ;
#include "AVRPorts.def"
#undef X
        default: return GPIOR0;
    }
}

template<Port port>
[[gnu::always_inline]]
[[nodiscard]] 
inline PortOutputRegister 
getOutputRegister() noexcept {
    return getOutputRegister(port);
}
template<Pin pin>
[[gnu::always_inline]]
[[nodiscard]] 
inline PortOutputRegister 
getOutputRegister() noexcept {
    return getOutputRegister<getPort(pin)>();
}

[[gnu::always_inline]]
[[nodiscard]] 
inline PortOutputRegister 
getOutputRegister(Pin pin) noexcept {
    return getOutputRegister(getPort(pin));
}
[[gnu::always_inline]]
[[nodiscard]] 
inline PortInputRegister 
getInputRegister(Port port) noexcept {
    switch (port) {
#define X(name) case Port :: name : return PIN ## name ;
#include "AVRPorts.def"
#undef X
        default: return GPIOR1;
    }
}
template<Port port>
[[gnu::always_inline]]
[[nodiscard]] 
inline PortInputRegister 
getInputRegister() noexcept {
    return getInputRegister(port);
}
template<Pin pin>
[[gnu::always_inline]]
[[nodiscard]] 
inline PortInputRegister 
getInputRegister() noexcept {
    return getInputRegister<getPort(pin)>();
}
[[gnu::always_inline]]
[[nodiscard]] 
inline PortInputRegister 
getInputRegister(Pin pin) noexcept {
    return getInputRegister(getPort(pin));
}
[[gnu::always_inline]]
[[nodiscard]] 
inline PortDirectionRegister 
getDirectionRegister(Port port) noexcept {
    switch (port) {
#define X(name) case Port:: name : return DDR ## name ;
#include "AVRPorts.def"
#undef X
        default: return GPIOR2;
    }
}
template<Port port>
[[gnu::always_inline]]
[[nodiscard]] 
inline PortDirectionRegister 
getDirectionRegister() noexcept {
    return getDirectionRegister(port);
}
[[gnu::always_inline]] inline 
void 
digitalWrite(Pin pin, decltype(LOW) value) noexcept { 
    if (isPhysicalPin(pin)) {
        if (auto &thePort = getOutputRegister(pin); value == LOW) {
            thePort = thePort & ~getPinMask(pin);
        } else {
            thePort = thePort | getPinMask(pin);
        }
    } else {
        ::digitalWrite(static_cast<byte>(pin), value); 
    }
}

[[gnu::always_inline]] 
inline decltype(LOW)
digitalRead(Pin pin) noexcept { 
    if (isPhysicalPin(pin)) {
        return (getInputRegister(pin) & getPinMask(pin)) ? HIGH : LOW;
    } else {
        return digitalRead(static_cast<byte>(pin));
    }
}
template<Pin pin>
[[gnu::always_inline]] 
inline decltype(LOW)
digitalRead() noexcept { 
    return digitalRead(pin);
}
[[gnu::always_inline]] inline 
void pinMode(Pin pin, decltype(INPUT) direction) noexcept {
    if (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } 
    // if the pin is not a physical one then don't expand it
}

template<Pin pin>
[[gnu::always_inline]] inline 
void pinMode(decltype(INPUT) direction) noexcept {
    if constexpr (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } 
    // if the pin is not a physical one then don't expand it
}

template<Pin pin>
[[gnu::always_inline]] 
inline void 
digitalWrite(decltype(LOW) value) noexcept { 
    digitalWrite(pin, value);
}
template<Pin pin, decltype(LOW) value>
[[gnu::always_inline]] 
inline void 
digitalWrite() noexcept { 
    digitalWrite<pin>(value);
}
template<Pin pin, decltype(LOW) to, decltype(HIGH) from>
[[gnu::always_inline]]
inline void
pulse() noexcept {
    digitalWrite<pin, to>();
    digitalWrite<pin, from>();
}

#endif // end SXCHIPSET_TYPE103_PINOUT_H

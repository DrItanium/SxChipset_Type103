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
Reset960,
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
    GPIOSelect = PortB4,

    INT0_ = PortD5,
    SEL = PortD6,
    INT3_ = PortD7,

    SD_EN = PortC2,
    PSRAM0 = PortC3,
    Ready = PortC4,
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
    DEN = Capture0, BE0 = Capture0,
    W_R_ = Capture1, BE1 = Capture1,
    FAIL = Capture2, BLAST_ = Capture2,
    Channel0_3 = Capture3, XIO_INT = Capture3,
    ADDR_INT0 = Capture4, DATA_INT0 = Capture4,
    ADDR_INT1 = Capture5, DATA_INT1 = Capture5,
    ADDR_INT2 = Capture6, RAM_IO = Capture6, 
    ADDR_INT3 = Capture7, Channel1_7 = Capture7,
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
constexpr Port getPort(Pin pin) noexcept {
    switch (pin) {
#define X(port, ind) case Pin :: Port ## port ## ind : return Port:: port 
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
[[gnu::always_inline]]
[[nodiscard]] constexpr decltype(auto) getPinMask(Pin pin) noexcept {
    switch (pin) {
#define PIN(port, offset) case Pin:: Port ## port ## offset : return _BV ( P ## port ## offset )
#define PORT(port) \
        PIN(port, 0); \
        PIN(port, 1); \
        PIN(port, 2); \
        PIN(port, 3); \
        PIN(port, 4); \
        PIN(port, 5); \
        PIN(port, 6); \
        PIN(port, 7)
        PORT(A);
        PORT(B);
        PORT(C);
        PORT(D);
#undef PIN
#undef PORT
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
        case Port::A: return PORTA;
        case Port::B: return PORTB;
        case Port::C: return PORTC;
        case Port::D: return PORTD;
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
        case Port::A: return PINA;
        case Port::B: return PINB;
        case Port::C: return PINC;
        case Port::D: return PIND;
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
        case Port::A: return DDRA;
        case Port::B: return DDRB;
        case Port::C: return DDRC;
        case Port::D: return DDRD;
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
[[gnu::always_inline]] inline void digitalWrite(Pin pin, decltype(LOW) value) noexcept;

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
[[gnu::always_inline]] inline void pinMode(Pin pin, decltype(INPUT) direction) noexcept;

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

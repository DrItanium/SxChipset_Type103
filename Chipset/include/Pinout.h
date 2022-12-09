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
#define DefPin(port, index) Port ## port ## index = PIN_P ## port ## index
#ifdef PIN_PB0
DefPin(B, 0),
#endif
#ifdef PIN_PB1
DefPin(B, 1),
#endif
#ifdef PIN_PB2
DefPin(B, 2),
#endif
#ifdef PIN_PB3
DefPin(B, 3),
#endif
#ifdef PIN_PB4
DefPin(B, 4),
#endif
#ifdef PIN_PB5
DefPin(B, 5),
#endif
#ifdef PIN_PB6
DefPin(B, 6),
#endif
#ifdef PIN_PB7
DefPin(B, 7),
#endif
#ifdef PIN_PC0
DefPin(C, 0),
#endif
#ifdef PIN_PC1
DefPin(C, 1),
#endif
#ifdef PIN_PC2
DefPin(C, 2),
#endif
#ifdef PIN_PC3
DefPin(C, 3),
#endif
#ifdef PIN_PC4
DefPin(C, 4),
#endif
#ifdef PIN_PC5
DefPin(C, 5),
#endif
#ifdef PIN_PC6
DefPin(C, 6),
#endif
#ifdef PIN_PC7
DefPin(C, 7),
#endif
#ifdef PIN_PA0
DefPin(A, 0),
#endif
#ifdef PIN_PA1
DefPin(A, 1),
#endif
#ifdef PIN_PA2
DefPin(A, 2),
#endif
#ifdef PIN_PA3
DefPin(A, 3),
#endif
#ifdef PIN_PA4
DefPin(A, 4),
#endif
#ifdef PIN_PA5
DefPin(A, 5),
#endif
#ifdef PIN_PA6
DefPin(A, 6),
#endif
#ifdef PIN_PA7
DefPin(A, 7),
#endif
#ifdef PIN_PD0
DefPin(D, 0),
#endif
#ifdef PIN_PD1
DefPin(D, 1),
#endif
#ifdef PIN_PD2
DefPin(D, 2),
#endif
#ifdef PIN_PD3
DefPin(D, 3),
#endif
#ifdef PIN_PD4
DefPin(D, 4),
#endif
#ifdef PIN_PD5
DefPin(D, 5),
#endif
#ifdef PIN_PD6
DefPin(D, 6),
#endif
#ifdef PIN_PD7
DefPin(D, 7),
#endif
#ifdef PIN_PE0
DefPin(E, 0),
#endif
#ifdef PIN_PE1
DefPin(E, 1),
#endif
#ifdef PIN_PE2
DefPin(E, 2),
#endif
#ifdef PIN_PE3
DefPin(E, 3),
#endif
#ifdef PIN_PE4
DefPin(E, 4),
#endif
#ifdef PIN_PE5
DefPin(E, 5),
#endif
#ifdef PIN_PE6
DefPin(E, 6),
#endif
#ifdef PIN_PE7
DefPin(E, 7),
#endif
#ifdef PIN_PF0
DefPin(F, 0),
#endif
#ifdef PIN_PF1
DefPin(F, 1),
#endif
#ifdef PIN_PF2
DefPin(F, 2),
#endif
#ifdef PIN_PF3
DefPin(F, 3),
#endif
#ifdef PIN_PF4
DefPin(F, 4),
#endif
#ifdef PIN_PF5
DefPin(F, 5),
#endif
#ifdef PIN_PF6
DefPin(F, 6),
#endif
#ifdef PIN_PF7
DefPin(F, 7),
#endif
#ifdef PIN_PG0
DefPin(G, 0),
#endif
#ifdef PIN_PG1
DefPin(G, 1),
#endif
#ifdef PIN_PG2
DefPin(G, 2),
#endif
#ifdef PIN_PG3
DefPin(G, 3),
#endif
#ifdef PIN_PG4
DefPin(G, 4),
#endif
#ifdef PIN_PG5
DefPin(G, 5),
#endif
#ifdef PIN_PG6
DefPin(G, 6),
#endif
#ifdef PIN_PG7
DefPin(G, 7),
#endif
#ifdef PIN_PH0
DefPin(H, 0),
#endif
#ifdef PIN_PH1
DefPin(H, 1),
#endif
#ifdef PIN_PH2
DefPin(H, 2),
#endif
#ifdef PIN_PH3
DefPin(H, 3),
#endif
#ifdef PIN_PH4
DefPin(H, 4),
#endif
#ifdef PIN_PH5
DefPin(H, 5),
#endif
#ifdef PIN_PH6
DefPin(H, 6),
#endif
#ifdef PIN_PH7
DefPin(H, 7),
#endif
#ifdef PIN_PJ0
DefPin(J, 0),
#endif
#ifdef PIN_PJ1
DefPin(J, 1),
#endif
#ifdef PIN_PJ2
DefPin(J, 2),
#endif
#ifdef PIN_PJ3
DefPin(J, 3),
#endif
#ifdef PIN_PJ4
DefPin(J, 4),
#endif
#ifdef PIN_PJ5
DefPin(J, 5),
#endif
#ifdef PIN_PJ6
DefPin(J, 6),
#endif
#ifdef PIN_PJ7
DefPin(J, 7),
#endif
#ifdef PIN_PK0
DefPin(K, 0),
#endif
#ifdef PIN_PK1
DefPin(K, 1),
#endif
#ifdef PIN_PK2
DefPin(K, 2),
#endif
#ifdef PIN_PK3
DefPin(K, 3),
#endif
#ifdef PIN_PK4
DefPin(K, 4),
#endif
#ifdef PIN_PK5
DefPin(K, 5),
#endif
#ifdef PIN_PK6
DefPin(K, 6),
#endif
#ifdef PIN_PK7
DefPin(K, 7),
#endif
#ifdef PIN_PL0
DefPin(L, 0),
#endif
#ifdef PIN_PL1
DefPin(L, 1),
#endif
#ifdef PIN_PL2
DefPin(L, 2),
#endif
#ifdef PIN_PL3
DefPin(L, 3),
#endif
#ifdef PIN_PL4
DefPin(L, 4),
#endif
#ifdef PIN_PL5
DefPin(L, 5),
#endif
#ifdef PIN_PL6
DefPin(L, 6),
#endif
#ifdef PIN_PL7
DefPin(L, 7),
#endif
#ifdef NUM_DIGITAL_PINS
Count = NUM_DIGITAL_PINS,
#else
#error "Count must equal the number of digital pins available"
#endif
#undef DefPin
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
#if defined(PORTA)
    A,
#endif
#if defined(PORTB)
    B,
#endif
#if defined(PORTC)
    C,
#endif
#if defined(PORTD)
    D,
#endif
#if defined(PORTE)
    E,
#endif
#if defined(PORTF)
    F,
#endif
#if defined(PORTG)
    G,
#endif
#if defined(PORTH)
    H,
#endif
#if defined(PORTJ)
    J,
#endif
#if defined(PORTK)
    K,
#endif
#if defined(PORTL)
    L,
#endif
    // stop at mega2560 tier
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
#ifdef PORTA
        Y(A);
#endif
#ifdef PORTB
        Y(B);
#endif
#ifdef PORTC
        Y(C);
#endif
#ifdef PORTD
        Y(D);
#endif
#ifdef PORTE
        Y(E);
#endif
#ifdef PORTF
        Y(F);
#endif
#ifdef PORTG
        Y(G);
#endif
#ifdef PORTH
        Y(H);
#endif
#ifdef PORTJ
        Y(J);
#endif
#ifdef PORTK
        Y(K);
#endif
#ifdef PORTL
        Y(L);
#endif
#undef Y
#undef X
        default: return Port::None;
    }
}
constexpr bool validPort(Port port) noexcept {
    switch (port) {
#if defined(PORTA)
        case Port::A:
#endif
#if defined(PORTB)
        case Port::B:
#endif
#if defined(PORTC)
        case Port::C:
#endif
#if defined(PORTD)
        case Port::D:
#endif
#if defined(PORTE)
        case Port::E:
#endif
#if defined(PORTF)
        case Port::F:
#endif
#if defined(PORTG)
        case Port::G:
#endif
#if defined(PORTH)
        case Port::H:
#endif
#if defined(PORTJ)
        case Port::J:
#endif
#if defined(PORTK)
        case Port::K:
#endif
#if defined(PORTL)
        case Port::L:
#endif
            return true;
        default:
            return false;
    }
}
[[gnu::always_inline]]
[[nodiscard]] constexpr decltype(auto) getPinMask(Pin pin) noexcept {
    switch (pin) {
#if 0
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
#endif
#define DefPin(port, offset) case Pin :: Port ## port ## offset : return _BV ( P ## port ## offset)
#ifdef PIN_PB0
DefPin(B, 0);
#endif
#ifdef PIN_PB1
DefPin(B, 1);
#endif
#ifdef PIN_PB2
DefPin(B, 2);
#endif
#ifdef PIN_PB3
DefPin(B, 3);
#endif
#ifdef PIN_PB4
DefPin(B, 4);
#endif
#ifdef PIN_PB5
DefPin(B, 5);
#endif
#ifdef PIN_PB6
DefPin(B, 6);
#endif
#ifdef PIN_PB7
DefPin(B, 7);
#endif
#ifdef PIN_PC0
DefPin(C, 0);
#endif
#ifdef PIN_PC1
DefPin(C, 1);
#endif
#ifdef PIN_PC2
DefPin(C, 2);
#endif
#ifdef PIN_PC3
DefPin(C, 3);
#endif
#ifdef PIN_PC4
DefPin(C, 4);
#endif
#ifdef PIN_PC5
DefPin(C, 5);
#endif
#ifdef PIN_PC6
DefPin(C, 6);
#endif
#ifdef PIN_PC7
DefPin(C, 7);
#endif
#ifdef PIN_PA0
DefPin(A, 0);
#endif
#ifdef PIN_PA1
DefPin(A, 1);
#endif
#ifdef PIN_PA2
DefPin(A, 2);
#endif
#ifdef PIN_PA3
DefPin(A, 3);
#endif
#ifdef PIN_PA4
DefPin(A, 4);
#endif
#ifdef PIN_PA5
DefPin(A, 5);
#endif
#ifdef PIN_PA6
DefPin(A, 6);
#endif
#ifdef PIN_PA7
DefPin(A, 7);
#endif
#ifdef PIN_PD0
DefPin(D, 0);
#endif
#ifdef PIN_PD1
DefPin(D, 1);
#endif
#ifdef PIN_PD2
DefPin(D, 2);
#endif
#ifdef PIN_PD3
DefPin(D, 3);
#endif
#ifdef PIN_PD4
DefPin(D, 4);
#endif
#ifdef PIN_PD5
DefPin(D, 5);
#endif
#ifdef PIN_PD6
DefPin(D, 6);
#endif
#ifdef PIN_PD7
DefPin(D, 7);
#endif
#ifdef PIN_PE0
DefPin(E, 0);
#endif
#ifdef PIN_PE1
DefPin(E, 1);
#endif
#ifdef PIN_PE2
DefPin(E, 2);
#endif
#ifdef PIN_PE3
DefPin(E, 3);
#endif
#ifdef PIN_PE4
DefPin(E, 4);
#endif
#ifdef PIN_PE5
DefPin(E, 5);
#endif
#ifdef PIN_PE6
DefPin(E, 6);
#endif
#ifdef PIN_PE7
DefPin(E, 7);
#endif
#ifdef PIN_PF0
DefPin(F, 0);
#endif
#ifdef PIN_PF1
DefPin(F, 1);
#endif
#ifdef PIN_PF2
DefPin(F, 2);
#endif
#ifdef PIN_PF3
DefPin(F, 3);
#endif
#ifdef PIN_PF4
DefPin(F, 4);
#endif
#ifdef PIN_PF5
DefPin(F, 5);
#endif
#ifdef PIN_PF6
DefPin(F, 6);
#endif
#ifdef PIN_PF7
DefPin(F, 7);
#endif
#ifdef PIN_PG0
DefPin(G, 0);
#endif
#ifdef PIN_PG1
DefPin(G, 1);
#endif
#ifdef PIN_PG2
DefPin(G, 2);
#endif
#ifdef PIN_PG3
DefPin(G, 3);
#endif
#ifdef PIN_PG4
DefPin(G, 4);
#endif
#ifdef PIN_PG5
DefPin(G, 5);
#endif
#ifdef PIN_PG6
DefPin(G, 6);
#endif
#ifdef PIN_PG7
DefPin(G, 7);
#endif
#ifdef PIN_PH0
DefPin(H, 0);
#endif
#ifdef PIN_PH1
DefPin(H, 1);
#endif
#ifdef PIN_PH2
DefPin(H, 2);
#endif
#ifdef PIN_PH3
DefPin(H, 3);
#endif
#ifdef PIN_PH4
DefPin(H, 4);
#endif
#ifdef PIN_PH5
DefPin(H, 5);
#endif
#ifdef PIN_PH6
DefPin(H, 6);
#endif
#ifdef PIN_PH7
DefPin(H, 7);
#endif
#ifdef PIN_PJ0
DefPin(J, 0);
#endif
#ifdef PIN_PJ1
DefPin(J, 1);
#endif
#ifdef PIN_PJ2
DefPin(J, 2);
#endif
#ifdef PIN_PJ3
DefPin(J, 3);
#endif
#ifdef PIN_PJ4
DefPin(J, 4);
#endif
#ifdef PIN_PJ5
DefPin(J, 5);
#endif
#ifdef PIN_PJ6
DefPin(J, 6);
#endif
#ifdef PIN_PJ7
DefPin(J, 7);
#endif
#ifdef PIN_PK0
DefPin(K, 0);
#endif
#ifdef PIN_PK1
DefPin(K, 1);
#endif
#ifdef PIN_PK2
DefPin(K, 2);
#endif
#ifdef PIN_PK3
DefPin(K, 3);
#endif
#ifdef PIN_PK4
DefPin(K, 4);
#endif
#ifdef PIN_PK5
DefPin(K, 5);
#endif
#ifdef PIN_PK6
DefPin(K, 6);
#endif
#ifdef PIN_PK7
DefPin(K, 7);
#endif
#ifdef PIN_PL0
DefPin(L, 0);
#endif
#ifdef PIN_PL1
DefPin(L, 1);
#endif
#ifdef PIN_PL2
DefPin(L, 2);
#endif
#ifdef PIN_PL3
DefPin(L, 3);
#endif
#ifdef PIN_PL4
DefPin(L, 4);
#endif
#ifdef PIN_PL5
DefPin(L, 5);
#endif
#ifdef PIN_PL6
DefPin(L, 6);
#endif
#ifdef PIN_PL7
DefPin(L, 7);
#endif
#undef DefPin
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

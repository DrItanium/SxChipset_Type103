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
/**
 * @brief Wrapper around the AVR Pins to make templating easier and cleaner
 */
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
    XIO5,
    XIO6,
    XIO7,
#ifdef PIN_WIRE_SDA
    SDA = PIN_WIRE_SDA,
#endif
#ifdef PIN_WIRE_SCL
    SCL = PIN_WIRE_SCL,
#endif
#ifdef PIN_SPI_SS
    SPI_CS = PIN_SPI_SS,
#endif
#ifdef PIN_SPI_SCK
    SPI_SCK = PIN_SPI_SCK,
#endif
#ifdef PIN_SPI_MOSI
    SPI_MOSI = PIN_SPI_MOSI,
#endif
#ifdef PIN_SPI_MISO
    SPI_MISO = PIN_SPI_MISO,
#endif
    // aliases
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
    Digital0 = PortB0,
    Digital1 = PortB1,
    Digital2 = PortB2,
    Digital3 = PortB3,
    Digital4 = PortB4,
    Digital5 = PortB5,
    Digital6 = PortB6,
    Digital7 = PortB7,
    Digital8 =  PortD0,
    Digital9 =  PortD1,
    Digital10 = PortD2,
    Digital11 = PortD3,
    Digital12 = PortD4,
    Digital13 = PortD5,
    Digital14 = PortD6,
    Digital15 = PortD7,
    Digital16 = PortC0,
    Digital17 = PortC1,
    Digital18 = PortC2,
    Digital19 = PortC3,
    Digital20 = PortC4,
    Digital21 = PortC5,
    Digital22 = PortC6,
    Digital23 = PortC7,
    Digital24 = PortA0,
    Digital25 = PortA1,
    Digital26 = PortA2,
    Digital27 = PortA3,
    Digital28 = PortA4,
    Digital29 = PortA5,
    Digital30 = PortA6,
    Digital31 = PortA7,
#elif defined(TYPE203_BOARD)
    Digital0 = PortE2,
    Digital1 = PortE7,
    Digital2 = PortE6,
    Digital3 = PortB7,
    Digital4 = PortB0,
    Digital5 = PortB2,
    Digital6 = PortB3,
    Digital7 = PortB1,
    Digital8 = PortE0,
    Digital9 = PortE1,
    Digital10 = PortD2,
    Digital11 = PortD3,
    Digital12 = PortD5,
    Digital13 = PortB5,
    Digital14 = PortB6,
    Digital15 = PortB4,
    Digital16 = PortD0,
    Digital17 = PortD1,
    Digital18 = PortD4,
    Digital19 = PortD6,
    Digital20 = PortD7,
    Digital21 = PortG5,
    Digital22 = PortG4,
    Digital23 = PortG3,
    Digital24 = PortK0,
    Digital25 = PortK1,
    Digital26 = PortK2,
    Digital27 = PortK3,
    Digital28 = PortK4,
    Digital29 = PortK5,
    Digital30 = PortK6,
    Digital31 = PortK7,
#else
#   error "Unknown board type defined!"
#endif
    Ready = Digital0,
    CLKO = Digital1,
    Enable = Digital2,
    CLKSignal = Digital3,
    GPIOSelect = Digital4,
    INT0_960_ = Digital15,

    Capture0 = Digital24,
    Capture1 = Digital25,
    Capture2 = Digital26,
    Capture3 = Digital27,
    Capture4 = Digital28,
    Capture5 = Digital29,
    Capture6 = Digital30,
    Capture7 = Digital31,
    // channel 0
    BE0 = Capture0,
    BE1 = Capture1,
    BLAST_ = Capture2,
    DEN = Capture3, 
    FAIL = Capture4, 
    DATA_INT0 = Capture5,
#   define X(x, y) ADR ## x = Capture ## y
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
#   undef X
    ShieldD0 = Digital8,
    ShieldD1 = Digital9,
    ShieldD2 = Digital10,
    ShieldD3 = Digital11,
    ShieldD4 = Digital12,
    ShieldD5 = Digital13,
    ShieldD6 = Digital14,
    ShieldD7 = Digital18,
    ShieldD8 = Digital19,
    ShieldD9 = Digital20,
    ShieldD10 = Digital21,
    ShieldD11 = Digital5,
    ShieldD12 = Digital6,
    ShieldD13 = Digital7,

    ShieldMOSI = ShieldD11,
    ShieldMISO = ShieldD12,
    ShieldSCK = ShieldD13,

    ShieldA0 = Digital22,
    ShieldA1 = Digital23,
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
#ifdef TYPE200_BOARD
    DataLower = K,
    DataUpper = F,
    Capture = J,
#endif
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
    Capture = A,
#elif defined(TYPE203_BOARD)
    Capture = K,
#endif
};
constexpr auto numberOfAvailablePins() noexcept {
    return 0 
#define X(a, b) + 1
#include "AVRPins.def"
#undef X
        ;
}
static_assert(numberOfAvailablePins() == NUM_DIGITAL_PINS);

constexpr auto numberOfAvailablePorts() noexcept {
    return 0
#define X(a) + 1
#include "AVRPorts.def"
#undef X
        ;
}

static_assert(numberOfAvailablePorts() > 0);

using PortOutputRegister = volatile byte&;
using PortInputRegister = volatile byte&;
using PortDirectionRegister = volatile byte&;
using PinState = decltype(LOW);
using PinDirection = decltype(OUTPUT);
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
digitalWrite(Pin pin, PinState value) noexcept { 
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
inline PinState 
digitalRead(Pin pin) noexcept { 
    if (isPhysicalPin(pin)) {
        return (getInputRegister(pin) & getPinMask(pin)) ? HIGH : LOW;
    } else {
        return ::digitalRead(static_cast<byte>(pin));
    }
}
template<Pin pin>
[[gnu::always_inline]] 
inline PinState
digitalRead() noexcept { 
    return digitalRead(pin);
}
[[gnu::always_inline]] inline 
void pinMode(Pin pin, PinDirection direction) noexcept {
    if (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } 
    // if the pin is not a physical one then don't expand it
}

template<Pin pin>
[[gnu::always_inline]] inline 
void pinMode(PinDirection direction) noexcept {
    if constexpr (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } 
    // if the pin is not a physical one then don't expand it
}

template<Pin pin>
[[gnu::always_inline]] 
inline void 
digitalWrite(PinState value) noexcept { 
    digitalWrite(pin, value);
}
template<Pin pin, PinState value>
[[gnu::always_inline]] 
inline void 
digitalWrite() noexcept { 
    digitalWrite<pin>(value);
}
template<Pin pin, PinState to, PinState from>
[[gnu::always_inline]]
inline void
pulse() noexcept {
    digitalWrite<pin, to>();
    digitalWrite<pin, from>();
}


constexpr bool hasPWM(Pin pin) noexcept {
    return digitalPinHasPWM(static_cast<byte>(pin));
}

template<Pin p>
constexpr auto HasPWM_v = hasPWM(p);

[[gnu::always_inline]]
inline uint8_t readFromCapture() noexcept {
    return getInputRegister<Port::Capture>();
}

#endif // end SXCHIPSET_TYPE103_PINOUT_H

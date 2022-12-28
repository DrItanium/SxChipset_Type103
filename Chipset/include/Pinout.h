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
#ifdef TYPE103_BOARD
    Ready = PortB0,
    CLKO = PortB1,
    Enable = PortB2,
    CLKSignal = PortB3,
    GPIOSelect = PortB4,
    INT0_960_ = PortD7,

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
#elif defined(TYPE200_BOARD)
    ShieldD0 = PortE0,
    ShieldD1 = PortE1,
    ShieldD2 = PortE4,
    ShieldD3 = PortE5,
    ShieldD4 = PortE6,
    ShieldD5 = PortH0,
    ShieldD6 = PortH1,
    ShieldD7 = PortH2,
    ShieldD8 = PortL0,
    ShieldD9 = PortL1,
    ShieldD10 = PortL2,
    ShieldD11 = SPI_MOSI,
    ShieldD12 = SPI_MISO,
    ShieldD13 = SPI_SCK,
    ShieldA0 = PortL4,
    ShieldA1 = PortL5,
    ShieldA2 = PortL6,
    ShieldA3 = PortL7,
    ShieldA4 = SDA,
    ShieldA5 = SCL,
    HLDA = ShieldA0,
    HOLD = ShieldA1,
    RESET960 = ShieldA2,
    Shield2_D0 = ShieldD0,
    Shield2_D1 = ShieldD1,
    Shield2_D2 = PortD2,
    Shield2_D3 = PortD3,
    Shield2_D4 = PortD4,
    Shield2_D5 = ShieldD5,
    Shield2_D6 = ShieldD6,
    Shield2_D7 = ShieldD7,
    Shield2_D8 = PortD5,
    Shield2_D9 = PortD6,
    Shield2_D10 = PortD7,
    Shield2_D11 = ShieldD11,
    Shield2_D12 = ShieldD12,
    Shield2_D13 = ShieldD13,
    Shield2_A0 = PortH4,
    Shield2_A1 = PortH5,
    Shield2_A2 = PortH6,
    Shield2_A3 = PortH7,
    Shield2_A4 = ShieldA4,
    Shield2_A5 = ShieldA5,
    Signal_Address = PortG5,
    AddressSel0 = PortG3,
    AddressSel1 = PortG4,
    Capture0 = PortJ0,
    Capture1 = PortJ1,
    Capture2 = PortJ2,
    Capture3 = PortJ3,
    Capture4 = PortJ4,
    Capture5 = PortJ5,
    Capture6 = PortJ6,
    Capture7 = PortJ7,
#   define X(x, y) ADR ## x = Capture ## y
    X(0, 0),
    X(1, 1),
    X(2, 2),
    X(3, 3),
    X(4, 4),
    X(5, 5),
    X(6, 6),
    X(7, 7),
    X(8, 0),
    X(9, 1),
    X(10, 2),
    X(11, 3),
    X(12, 4),
    X(13, 5),
    X(14, 6),
    X(15, 7),
    X(16, 0),
    X(17, 1),
    X(18, 2),
    X(19, 3),
    X(20, 4),
    X(21, 5),
    X(22, 6),
    X(23, 7),
    X(24, 0),
    X(25, 1),
    X(26, 2),
    X(27, 3),
    X(28, 4),
    X(29, 5),
    X(30, 6),
    X(31, 7),
#   undef X
    Data0 = PortK0,
    Data1 = PortK1,
    Data2 = PortK2,
    Data3 = PortK3,
    Data4 = PortK4,
    Data5 = PortK5,
    Data6 = PortK6,
    Data7 = PortK7,
    Data8 = PortF0,
    Data9 = PortF1,
    Data10 = PortF2,
    Data11 = PortF3,
    Data12 = PortF4,
    Data13 = PortF5,
    Data14 = PortF6,
    Data15 = PortF7,
    BE0 = Capture0,
    BE1 = Capture1,
    BLAST_ = Capture2,
    DEN = Capture3, 
    FAIL = Capture4, 
    W_R_ = Capture5,

    Ready = PortE2,
    CLKO = PortE7,
    INT0_960_ = PortB5,
    INT1_960 = PortE3,
    INT2_960 = PortH3,
    INT3_960_ = PortL3,
    PSRAM0 = Shield2_D3,
#else
#   error "Unknown board type defined!"
#endif
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
#ifdef TYPE103_BOARD
    Capture = A,
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

#endif // end SXCHIPSET_TYPE103_PINOUT_H

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
#ifdef LED_BUILTIN
    LED = LED_BUILTIN,
#endif
    SD_EN = PortB0,
    INT0_960_ = PortB5, // OC1A
    HOLD = PortB7,

    HLDA = PortD3,
    Lock = PortD4,
    Reset = PortD5,
    ReadyDirect = PortD6,

    IsMemorySpaceOperation = PortE2,
    CLK1 = PortE3,
    NewTransaction = PortE4,
    WriteTransaction = PortE5,
    ExternalMemoryOperation = PortE6,

    CLK2 = PortE7, // hardwired for this purpose
    EBI_WR = PortG0,
    EBI_RD = PortG1,
    EBI_ALE = PortG2,
    BE0 = PortG3,
    BE1 = PortG4,
    BLAST = PortG5,

    ONE_SHOT_READY = PortH6, // OC2B





};
enum class Port : byte {
    // stop at mega2560 tier
#define X(name) name ,
#include "AVRPorts.def"
#undef X
    None,
    ExecutionState = D,
    DataLinesLower = F,
    DataLinesUpper = K,
    AddressLinesLowest = L,
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
[[gnu::always_inline]]
//[[gnu::noinline]]
inline void
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
    if constexpr (isPhysicalPin(pin)) {
        return (getInputRegister<pin>() & getPinMask<pin>()) ? HIGH : LOW;
    } else {
        return ::digitalRead(pin);
    }
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
    if constexpr (isPhysicalPin(pin)) {
        if (auto& thePort = getOutputRegister<pin>(); value == LOW) {
            thePort = thePort & ~getPinMask<pin>();
        } else {
            thePort = thePort | getPinMask<pin>();
        }
    } else {
        digitalWrite(pin, value);
    }
}
template<Pin pin, PinState value>
[[gnu::always_inline]]
inline void
digitalWrite() noexcept {
    if constexpr (isPhysicalPin(pin)) {
        if constexpr (auto& thePort = getOutputRegister<pin>(); value == LOW) {
            thePort = thePort & ~getPinMask<pin>();
        } else {
            thePort = thePort | getPinMask<pin>();
        }
    } else {
        digitalWrite<pin>(value);
    }
}
template<Pin pin, PinState to, PinState from>
[[gnu::always_inline]]
inline void
pulse() noexcept {
    digitalWrite<pin, to>();
    digitalWrite<pin, from>();
}

/**
 * @brief Toggle the value of the output pin blindly
 * @tparam pin The output pin to toggle
 */
template<Pin pin>
[[gnu::always_inline]]
inline void
toggle() noexcept {
    getInputRegister<pin>() |= getPinMask<pin>();
}

/**
 * @brief Do a blind toggle of the given output pin by using the input register; This will always return the pin to its original position without needing to be explicit
 * @tparam pin The pin to toggle
 */
template<Pin pin>
[[gnu::always_inline]]
inline void
pulse() noexcept {
    toggle<pin>();
    toggle<pin>();
}


constexpr bool hasPWM(Pin pin) noexcept {
    return digitalPinHasPWM(static_cast<byte>(pin));
}

template<Pin p>
constexpr auto HasPWM_v = hasPWM(p);

template<Pin p>
[[gnu::always_inline]]
inline bool sampleOutputState() noexcept {
    // we want to look at what we currently have the output register set to
    return (getOutputRegister<p>() & getPinMask<p>()) != 0;
}

inline void analogWrite(Pin pin, int value) noexcept {
    ::analogWrite(static_cast<int>(pin), value);
}
#endif // end SXCHIPSET_TYPE103_PINOUT_H

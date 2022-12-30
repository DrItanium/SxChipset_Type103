/*
i960SxChipset_Type103
Copyright (c) 2022-2023, Joshua Scoggins
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
#include "Detect.h"
#include "Setup.h"
#include "Types.h"

#ifdef TYPE200_BOARD
#include "Setup.h"
#include "Types.h"
#include "Pinout.h"
#include "Peripheral.h"

void 
Platform::doReset(decltype(LOW) value) noexcept {
    digitalWrite<Pin::RESET960>(value);
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    digitalWrite<Pin::HOLD>(value);
}

void
Platform::begin() noexcept {
    if (!initialized_) {
        initialized_ = true;
        // configure pins
        //pinMode<Pin::GPIOSelect>(OUTPUT);
        pinMode<Pin::SD_EN>(OUTPUT);
        pinMode<Pin::PSRAM0>(OUTPUT);
        pinMode<Pin::Ready>(OUTPUT);
        pinMode<Pin::INT0_960_>(OUTPUT);
        pinMode<Pin::INT1_960>(OUTPUT);
        pinMode<Pin::INT2_960>(OUTPUT);
        pinMode<Pin::INT3_960_>(OUTPUT);
        pinMode<Pin::DEN>(INPUT);
        pinMode<Pin::BLAST_>(INPUT);
        pinMode<Pin::FAIL>(INPUT);
        pinMode<Pin::Data0>(INPUT);
        pinMode<Pin::Data1>(INPUT);
        pinMode<Pin::Data2>(INPUT);
        pinMode<Pin::Data3>(INPUT);
        pinMode<Pin::Data4>(INPUT);
        pinMode<Pin::Data5>(INPUT);
        pinMode<Pin::Data6>(INPUT);
        pinMode<Pin::Data7>(INPUT);
        pinMode<Pin::Data8>(INPUT);
        pinMode<Pin::Data9>(INPUT);
        pinMode<Pin::Data10>(INPUT);
        pinMode<Pin::Data11>(INPUT);
        pinMode<Pin::Data12>(INPUT);
        pinMode<Pin::Data13>(INPUT);
        pinMode<Pin::Data14>(INPUT);
        pinMode<Pin::Data15>(INPUT);
        pinMode<Pin::Capture0>(INPUT);
        pinMode<Pin::Capture1>(INPUT);
        pinMode<Pin::Capture2>(INPUT);
        pinMode<Pin::Capture3>(INPUT);
        pinMode<Pin::Capture4>(INPUT);
        pinMode<Pin::Capture5>(INPUT);
        pinMode<Pin::Capture6>(INPUT);
        pinMode<Pin::Capture7>(INPUT);
        pinMode<Pin::AddressSel0>(OUTPUT);
        pinMode<Pin::AddressSel1>(OUTPUT);
        pinMode<Pin::Signal_Address>(OUTPUT);
        pinMode<Pin::RESET960>(OUTPUT);
        pinMode<Pin::HOLD>(OUTPUT);
        pinMode<Pin::SPI_MISO>(INPUT_PULLUP);
        digitalWrite<Pin::Ready, HIGH>();
        digitalWrite<Pin::INT0_960_, HIGH>();
        digitalWrite<Pin::INT1_960, LOW>();
        digitalWrite<Pin::INT2_960, LOW>();
        digitalWrite<Pin::INT3_960_, HIGH>();
        digitalWrite<Pin::PSRAM0, HIGH>();
        digitalWrite<Pin::SD_EN, HIGH>();
        digitalWrite<Pin::AddressSel0>(LOW);
        digitalWrite<Pin::AddressSel1>(LOW);
        digitalWrite<Pin::Signal_Address>(LOW);
        doReset(LOW);
        doHold(LOW);
        // do an initial clear of the clock signal
    }
}

void 
Platform::startAddressTransaction() noexcept {
    // clear the address counter to be on the safe side
    digitalWrite<Pin::AddressSel0>(LOW);
    digitalWrite<Pin::AddressSel1>(LOW);
    digitalWrite<Pin::Signal_Address>(LOW);
    singleCycleDelay(); // introduce this extra cycle of delay to make sure
                        // that inputs are updated correctly since they are
                        // tristated
}
void
Platform::endAddressTransaction() noexcept {
    digitalWrite<Pin::Signal_Address>(HIGH);
    singleCycleDelay();
    isReadOperation_ = digitalRead<Pin::W_R_>() == LOW;
    if (isReadOperation_) {
        getDirectionRegister<Port::DataLower>() = 0;
        getDirectionRegister<Port::DataUpper>() = 0;
    } else {
        getDirectionRegister<Port::DataLower>() = 0xFF;
        getDirectionRegister<Port::DataUpper>() = 0xFF;
    }
}
void
Platform::collectAddress() noexcept {
    address_.bytes[3] = readInputChannelAs<uint8_t>();
    digitalWrite<Pin::AddressSel0>(HIGH);
    singleCycleDelay();
    address_.bytes[2] = readInputChannelAs<uint8_t>();
    digitalWrite<Pin::AddressSel0>(LOW);
    digitalWrite<Pin::AddressSel1>(HIGH);
    singleCycleDelay();
    address_.bytes[1] = readInputChannelAs<uint8_t>();
    digitalWrite<Pin::AddressSel0>(HIGH);
    singleCycleDelay();
    address_.bytes[0] = readInputChannelAs<uint8_t>();
    singleCycleDelay();
}

void
Platform::startInlineSPIOperation() noexcept {
}

void
Platform::endInlineSPIOperation() noexcept {
}

[[gnu::always_inline]]
inline uint16_t getDataLines() noexcept {
    SplitWord16 result{0};
    result.bytes[0] = getInputRegister<Port::DataLower>();
    result.bytes[1] = getInputRegister<Port::DataUpper>();
    return result.full;
}
[[gnu::always_inline]]
inline void setDataLines(SplitWord16 value) noexcept {
    getOutputRegister<Port::DataLower>() = value.bytes[0];
    getOutputRegister<Port::DataUpper>() = value.bytes[1];
}

uint16_t 
Platform::getDataLines(const Channel0Value& c1, InlineSPI) noexcept {
    return ::getDataLines();
}
uint16_t 
Platform::getDataLines(const Channel0Value& c1, NoInlineSPI) noexcept {
    return ::getDataLines();
}
void 
Platform::setDataLines(uint16_t value, InlineSPI) noexcept {
    ::setDataLines(SplitWord16{value});
}
void 
Platform::setDataLines(uint16_t value, NoInlineSPI) noexcept {
    ::setDataLines(SplitWord16{value});
}

#endif

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
        pinMode<Pin::GPIOSelect>(OUTPUT);
        pinMode<Pin::SD_EN>(OUTPUT);
        pinMode<Pin::PSRAM0>(OUTPUT);
        pinMode<Pin::Ready>(OUTPUT);
        pinMode<Pin::INT0_960_>(OUTPUT);
        pinMode<Pin::Enable>(OUTPUT);
        pinMode<Pin::CLKSignal>(OUTPUT);
        pinMode<Pin::DEN>(INPUT);
        pinMode<Pin::BLAST_>(INPUT);
        pinMode<Pin::FAIL>(INPUT);
        pinMode<Pin::Capture0>(INPUT);
        pinMode<Pin::Capture1>(INPUT);
        pinMode<Pin::Capture2>(INPUT);
        pinMode<Pin::Capture3>(INPUT);
        pinMode<Pin::Capture4>(INPUT);
        pinMode<Pin::Capture5>(INPUT);
        pinMode<Pin::Capture6>(INPUT);
        pinMode<Pin::Capture7>(INPUT);
        digitalWrite<Pin::CLKSignal, LOW>();
        digitalWrite<Pin::Ready, HIGH>();
        digitalWrite<Pin::GPIOSelect, HIGH>();
        digitalWrite<Pin::INT0_960_, HIGH>();
        digitalWrite<Pin::PSRAM0, HIGH>();
        digitalWrite<Pin::SD_EN, HIGH>();
        digitalWrite<Pin::Enable, HIGH>();
        // do an initial clear of the clock signal
        pulse<Pin::CLKSignal, LOW, HIGH>();
    }
}

[[gnu::always_inline]] 
inline void 
triggerClock() noexcept {
    pulse<Pin::CLKSignal, LOW, HIGH>();
    singleCycleDelay();
}
void 
Platform::startAddressTransaction() noexcept {
    // clear the address counter to be on the safe side
    triggerClock();
    digitalWrite<Pin::Enable, LOW>();
    singleCycleDelay(); // introduce this extra cycle of delay to make sure
                        // that inputs are updated correctly since they are
                        // tristated
}
void
Platform::endAddressTransaction() noexcept {
    digitalWrite<Pin::Enable, HIGH>();
    triggerClock();
}
void
Platform::collectAddress() noexcept {
    auto m2 = readInputChannelAs<Channel2Value>();
    address_.bytes[0] = m2.getWholeValue();
    address_.address.a0 = 0;
    triggerClock();
    address_.bytes[1] = readInputChannelAs<uint8_t>();
    triggerClock();
    address_.bytes[2] = readInputChannelAs<uint8_t>();
    triggerClock();
    address_.bytes[3] = readInputChannelAs<uint8_t>();
    isReadOperation_ = m2.isReadOperation();
    auto direction = isReadOperation_ ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    if (direction != dataLinesDirection_) {
        dataLinesDirection_ = direction;
        MCP23S17::writeDirection<DataLines, Pin::GPIOSelect>(dataLinesDirection_);
    }
}

void
Platform::startInlineSPIOperation() noexcept {
}

void
Platform::endInlineSPIOperation() noexcept {
}


uint16_t 
Platform::getDataLines(const Channel0Value& c1, InlineSPI) noexcept {
    SplitWord16 result{0};
    result.bytes[0] = getInputRegister<Port::DataLower>();
    result.bytes[1] = getInputRegister<Port::DataUpper>();
    return result.full;
}
uint16_t 
Platform::getDataLines(const Channel0Value& c1, NoInlineSPI) noexcept {
    SplitWord16 result{0};
    result.bytes[0] = getInputRegister<Port::DataLower>();
    result.bytes[1] = getInputRegister<Port::DataUpper>();
    return result.full;
}
void 
Platform::setDataLines(uint16_t value, InlineSPI) noexcept {
    SplitWord16 tmp{value};
    getOutputRegister<Port::DataLower>() = tmp.bytes[0];
    getOutputRegister<Port::DataUpper>() = tmp.bytes[1];
}
void 
Platform::setDataLines(uint16_t value, NoInlineSPI) noexcept {
    SplitWord16 tmp{value};
    getOutputRegister<Port::DataLower>() = tmp.bytes[0];
    getOutputRegister<Port::DataUpper>() = tmp.bytes[1];
}

#endif

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
doReset(decltype(LOW) value) noexcept {
    digitalWrite<Pin::Reset960>(value);
}
void 
doHold(decltype(LOW) value) noexcept {
    digitalWrite<Pin::HOLD>(value);
}

void
configurePins() noexcept {
    // configure pins
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
    pinMode<Pin::Capture0>(INPUT);
    pinMode<Pin::Capture1>(INPUT);
    pinMode<Pin::Capture2>(INPUT);
    pinMode<Pin::Capture3>(INPUT);
    pinMode<Pin::Capture4>(INPUT);
    pinMode<Pin::Capture5>(INPUT);
    pinMode<Pin::Capture6>(INPUT);
    pinMode<Pin::Capture7>(INPUT);
    digitalWrite<Pin::Ready, HIGH>();
    digitalWrite<Pin::GPIOSelect, HIGH>();
    digitalWrite<Pin::INT0_960_, HIGH>();
    digitalWrite<Pin::PSRAM0, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
}
void
setupAddressAndDataLines() noexcept {
    configurePins();
}

void 
enterTransactionSetup() noexcept {
}
void
leaveTransactionSetup() noexcept {
}
SplitWord32 
configureTransaction() noexcept {
    SplitWord32 addr { 0 };
#if 0
    auto m2 = readInputChannelAs<Channel2Value>();
    addr.bytes[0] = m2.getWholeValue();
    addr.address.a0 = 0;
    triggerClock();
    addr.bytes[1] = readInputChannelAs<uint8_t>();
    triggerClock();
    addr.bytes[2] = readInputChannelAs<uint8_t>();
    triggerClock();
    addr.bytes[3] = readInputChannelAs<uint8_t>();
    auto direction = m2.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    if (direction != dataLinesDirection) {
        dataLinesDirection = direction;
        MCP23S17::writeDirection<DataLines>(dataLinesDirection);
    }
#endif
    return addr;
}
bool
isReadOperation() noexcept {
    return dataLinesDirection == MCP23S17::AllOutput16;
}

uint16_t dataLinesDirection = MCP23S17::AllInput16;

template<bool busHeldOpen>
[[gnu::always_inline]] 
inline uint16_t 
getDataLines(const Channel0Value& c1) noexcept {
    if (c1.dataInterruptTriggered()) {
        if constexpr (busHeldOpen) {
#ifdef AVR_SPI_AVAILABLE
            SPDR = 0;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
            auto value = SPDR;
            SPDR = 0;
            asm volatile ("nop");
            previousValue.bytes[0] = value;
            while (!(SPSR & _BV(SPIF))) ; // wait
            previousValue.bytes[1] = SPDR;
#else
            previousValue.bytes[0] = SPI.transfer(0);
            previousValue.bytes[1] = SPI.transfer(0);
#endif
        } else {
            previousValue.full = MCP23S17::readGPIO16<DataLines>();
        }
    }
    return previousValue.full;
}

template<bool busHeldOpen>
[[gnu::always_inline]] 
inline void 
setDataLinesOutput(uint16_t value) noexcept {
    if (currentDataLinesValue != value) {
        currentDataLinesValue = value;
        if constexpr (busHeldOpen) {
#ifdef AVR_SPI_AVAILABLE
            SPDR = static_cast<byte>(value);
            asm volatile ("nop");
            auto next = static_cast<byte>(value >> 8);
            while (!(SPSR & _BV(SPIF))) ; // wait
            SPDR = next;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
#else
            SPI.transfer(static_cast<byte>(value));
            SPI.transfer(static_cast<byte>(value >> 8));
#endif
        } else {
            MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
        }
    }
}

void
startInlineSPIOperation(bool isReadOperation) {
    digitalWrite<Pin::GPIOSelect, LOW>();
    auto TargetAction = isReadOperation ? MCP23S17::WriteOpcode_v<DataLines> : MCP23S17::ReadOpcode_v<DataLines>;
    auto TargetRegister = static_cast<byte>(isReadOperation ? MCP23S17::Registers::OLAT : MCP23S17::Registers::GPIO);
#ifdef AVR_SPI_AVAILABLE
    SPDR = TargetAction;
    asm volatile ("nop");
    while (!(SPSR & _BV(SPIF))); 
    SPDR = TargetRegister;
    asm volatile ("nop");
    while (!(SPSR & _BV(SPIF))); 
#else
    SPI.transfer(TargetAction);
    SPI.transfer(TargetRegister);
#endif
}

void
endInlineSPIOperation() {
    digitalWrite<Pin::GPIOSelect, HIGH>();
}


uint16_t getDataLines(const Channel0Value& m0, InlineSPI) noexcept {
    return getDataLines<true>(m0);
}
uint16_t getDataLines(const Channel0Value& m0, NoInlineSPI) noexcept {
    return getDataLines<false>(m0);
}
void setDataLines(uint16_t value, InlineSPI) noexcept {
    setDataLinesOutput<true>(value);
}
void setDataLines(uint16_t value, NoInlineSPI) noexcept {
    setDataLinesOutput<false>(value);
}
#endif

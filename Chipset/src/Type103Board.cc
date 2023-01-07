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

#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
#include "Setup.h"
#include "Types.h"
#include "Pinout.h"
#include "MCP23S17.h"
#include "Peripheral.h"

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
/**
 * @brief Onboard device to control reset and other various features
 */
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device7;

void 
Platform::doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~0b10;
    } else {
        theGPIO |= 0b10;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
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
        MCP23S17::IOCON reg;
        reg.mirrorInterruptPins();
        reg.treatDeviceAsOne16BitPort();
        reg.enableHardwareAddressing();
        reg.interruptIsActiveLow();
        reg.configureInterruptsAsActiveDriver();
        reg.disableSequentialOperation();
        // at the start all of the io expanders will respond to the same address
        // so first just make sure we write out the initial iocon
        MCP23S17::writeIOCON<MCP23S17::HardwareDeviceAddress::Device0, Pin::GPIOSelect>(reg);
        // now make sure that everything is configured correctly initially
        MCP23S17::writeIOCON<DataLines, Pin::GPIOSelect>(reg);
        MCP23S17::writeDirection<DataLines, Pin::GPIOSelect>(MCP23S17::AllInput16);
        MCP23S17::write16<DataLines, MCP23S17::Registers::GPINTEN, Pin::GPIOSelect>(0xFFFF);
        reg.mirrorInterruptPins();
        MCP23S17::writeIOCON<XIO, Pin::GPIOSelect>(reg);
        MCP23S17::writeDirection<XIO, Pin::GPIOSelect>(0b1000'0100, 0b1111'1111);
        // setup the extra interrupts as well (hooked in through xio)
        MCP23S17::writeGPIO8_PORTA<XIO, Pin::GPIOSelect>(0b0010'0000); 
        MCP23S17::write16<XIO, MCP23S17::Registers::GPINTEN, Pin::GPIOSelect>(0x0000); // no
                                                                                       // interrupts
        // make sure that we clear out any interrupts
        //(void)MCP23S17::readGPIO16<DataLines, Pin::GPIOSelect>();
        dataLinesDirection_ = MCP23S17::AllInput16;
        previousValue_.setWholeValue(0);
        MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT, Pin::GPIOSelect>(previousValue_.full);
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
    isReadOperation_ = m2.isReadOperation();
    if (auto direction = isReadOperation_ ? MCP23S17::AllOutput16 : MCP23S17::AllInput16; direction != dataLinesDirection_) {
        digitalWrite<Pin::GPIOSelect, LOW>();

        SPDR = MCP23S17::WriteOpcode_v<DataLines>;
        asm volatile ("nop");
        dataLinesDirection_ = direction;
        // inline updating the direction while getting the address bits
        address_.bytes[0] = m2.getWholeValue();
        address_.address.a0 = 0;
        triggerClock();
        auto opcode = static_cast<byte>(MCP23S17::Registers::IODIR);
        while (!(SPSR & _BV(SPIF)));

        SPDR = opcode;
        asm volatile ("nop");
        address_.bytes[1] = readInputChannelAs<uint8_t>();
        triggerClock();
        auto first = static_cast<byte>(dataLinesDirection_);
        while (!(SPSR & _BV(SPIF)));

        SPDR = first;
        asm volatile ("nop");
        address_.bytes[2] = readInputChannelAs<uint8_t>();
        triggerClock();
        auto second = static_cast<byte>(dataLinesDirection_ >> 8);
        while (!(SPSR & _BV(SPIF)));

        SPDR = second ;
        asm volatile ("nop") ;
        address_.bytes[3] = readInputChannelAs<uint8_t>();
        while (!(SPSR & _BV(SPIF)));

        digitalWrite<Pin::GPIOSelect, HIGH>();
        //MCP23S17::writeDirection<DataLines, Pin::GPIOSelect>(dataLinesDirection_);
    } else {
        address_.bytes[0] = m2.getWholeValue();
        address_.address.a0 = 0;
        triggerClock();
        address_.bytes[1] = readInputChannelAs<uint8_t>();
        triggerClock();
        address_.bytes[2] = readInputChannelAs<uint8_t>();
        triggerClock();
        address_.bytes[3] = readInputChannelAs<uint8_t>();
    }
}

void
Platform::startInlineSPIOperation() noexcept {
    digitalWrite<Pin::GPIOSelect, LOW>();
    auto TargetAction = isReadOperation_ ? MCP23S17::WriteOpcode_v<DataLines> : MCP23S17::ReadOpcode_v<DataLines>;
    auto TargetRegister = static_cast<byte>(isReadOperation_ ? MCP23S17::Registers::OLAT : MCP23S17::Registers::GPIO);
    // starting a new transaction so reset the starting index to 0
    lastSPIOffsetIndex_ = 0;
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
Platform::endInlineSPIOperation() noexcept {
    digitalWrite<Pin::GPIOSelect, HIGH>();
}


uint16_t 
Platform::getDataLines(const Channel0Value& c1, InlineSPI) noexcept {
    if (c1.dataInterruptTriggered()) {
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
    }
    return previousValue.full;
}
uint16_t 
Platform::getDataLines(const Channel0Value& c1, NoInlineSPI) noexcept {
    if (c1.dataInterruptTriggered()) {
        previousValue_.full = MCP23S17::readGPIO16<DataLines, Pin::GPIOSelect>();
    }
    return previousValue_.full;
}
void 
Platform::setDataLines(uint16_t value, InlineSPI) noexcept {
    if (previousValue_ != value) {
        previousValue_.full = value;
#ifdef AVR_SPI_AVAILABLE
        SPDR = previousValue_.bytes[0];
        asm volatile ("nop");
        auto next = previousValue_.bytes[1];
        while (!(SPSR & _BV(SPIF))); // wait
        SPDR = next;
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))); // wait
#else
        SPI.transfer(previousValue_.bytes[0]);
        SPI.transfer(previousValue_.bytes[1]);
#endif
    }
}
void 
Platform::setDataLines(uint16_t value, NoInlineSPI) noexcept {
    if (previousValue_ != value) {
        previousValue_.full = value;
        MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT, Pin::GPIOSelect>(previousValue_.full);
    }
}
#endif

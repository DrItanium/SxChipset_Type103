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
#include "Pinout.h"
#include "MCP23S17.h"
#include "Peripheral.h"
#include "xmem.h"

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
        dataLinesDirection_ = MCP23S17::AllInput8;
        previousValue_.setWholeValue(0);
        MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT, Pin::GPIOSelect>(previousValue_.full);
        MCP23S17::writeDirection<DataLines, Pin::GPIOSelect>(dataLinesDirection_, dataLinesDirection_);
        xmem::begin(true);
        // setup the direct data lines to be input
        getDirectionRegister<Port::DataLower>() = 0;
        getDirectionRegister<Port::DataUpper>() = 0;
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
void Platform::startInlineSPIOperation() noexcept { }
void Platform::endInlineSPIOperation() noexcept { }

void
Platform::collectAddress() noexcept {
    auto m2 = readInputChannelAs<Channel2Value>();
    isReadOperation_ = m2.isReadOperation();
    auto tmp = isReadOperation_ ? 0xFF : 0x00;
    address_.bytes[0] = m2.getWholeValue();
    address_.address.a0 = 0;
    triggerClock();
    address_.bytes[1] = readInputChannelAs<uint8_t>();
    triggerClock();
    address_.bytes[2] = readInputChannelAs<uint8_t>();
    triggerClock();
    address_.bytes[3] = readInputChannelAs<uint8_t>();
    getDirectionRegister<Port::DataLower>() = tmp;
    getDirectionRegister<Port::DataUpper>() = tmp;
}
uint16_t
Platform::getDataLines(const Channel0Value&, InlineSPI) noexcept {
    return makeWord(getInputRegister<Port::DataUpper>(), getInputRegister<Port::DataLower>());
}
uint16_t
Platform::getDataLines(const Channel0Value&, NoInlineSPI) noexcept {
    return makeWord(getInputRegister<Port::DataUpper>(), getInputRegister<Port::DataLower>());
}
void
Platform::setDataLines(uint16_t value, InlineSPI) noexcept {
    getOutputRegister<Port::DataLower>() = lowByte(value);
    getOutputRegister<Port::DataUpper>() = highByte(value);
}
void
Platform::setDataLines(uint16_t value, NoInlineSPI) noexcept {
    getOutputRegister<Port::DataLower>() = lowByte(value);
    getOutputRegister<Port::DataUpper>() = highByte(value);
}

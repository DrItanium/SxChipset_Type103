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

#ifdef TYPE103_BOARD
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
inline void 
doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}
[[gnu::always_inline]] inline void 
doHold(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~0b10;
    } else {
        theGPIO |= 0b10;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}

void
setupIOExpanders() noexcept {
    MCP23S17::IOCON reg;
    reg.mirrorInterruptPins();
    reg.treatDeviceAsOne16BitPort();
    reg.enableHardwareAddressing();
    reg.interruptIsActiveLow();
    reg.configureInterruptsAsActiveDriver();
    reg.disableSequentialOperation();
    // at the start all of the io expanders will respond to the same address
    // so first just make sure we write out the initial iocon
    MCP23S17::writeIOCON<MCP23S17::HardwareDeviceAddress::Device0>(reg);
    // now make sure that everything is configured correctly initially
    MCP23S17::writeIOCON<DataLines>(reg);
    MCP23S17::writeDirection<DataLines>(MCP23S17::AllInput16);
    MCP23S17::write16<DataLines, MCP23S17::Registers::GPINTEN>(0xFFFF);
    reg.mirrorInterruptPins();
    MCP23S17::writeIOCON<XIO>(reg);
    MCP23S17::writeDirection<XIO>(0b1000'0100, 0b1111'1111);
    // setup the extra interrupts as well (hooked in through xio)
    MCP23S17::writeGPIO8_PORTA<XIO>(0b0010'0000); 
    MCP23S17::write16<XIO, MCP23S17::Registers::GPINTEN>(0x0000); // no
                                                                  // interrupts

    dataLinesDirection = MCP23S17::AllInput16;
    currentDataLinesValue = 0;
    MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
}
void
configurePins() noexcept {
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
void
setupAddressAndDataLines() noexcept {
    setupIOExpanders();
    configurePins();
}

[[gnu::always_inline]] 
inline void 
triggerClock() noexcept {
    pulse<Pin::CLKSignal, LOW, HIGH>();
    singleCycleDelay();
}
void 
enterTransactionSetup() noexcept {
    // clear the address counter to be on the safe side
    triggerClock();
    digitalWrite<Pin::Enable, LOW>();
    singleCycleDelay(); // introduce this extra cycle of delay to make sure
                        // that inputs are updated correctly since they are
                        // tristated
}
inline void
leaveTransactionSetup() noexcept {
    digitalWrite<Pin::Enable, HIGH>();
    triggerClock();
}
SplitWord32 
configureTransaction() noexcept {
    SplitWord32 addr { 0 };
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
    return addr;
}
bool
isReadOperation() noexcept {
    return dataLinesDirection == MCP23S17::AllOutput16;
}

uint16_t dataLinesDirection = MCP23S17::AllInput16;

template<bool busHeldOpen, bool ignoreInterrupts = true>
[[gnu::always_inline]] 
inline uint16_t 
getDataLines(const Channel0Value& c1) noexcept {
    if (ignoreInterrupts || c1.dataInterruptTriggered()) {
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

class CacheOperationHandler : public OperationHandler {
    public:
        using Parent = OperationHandler;
        ~CacheOperationHandler() override = default;
        void
        startTransaction(const SplitWord32& addr) noexcept override {
            Parent::startTransaction(addr);
            line_ = &getCache().find(addr);
            // in this case we are going to be starting an inline spi
            // transaction
            digitalWrite<Pin::GPIOSelect, LOW>();
            static constexpr auto TargetAction = isReadOperation ? MCP23S17::WriteOpcode_v<DataLines> : MCP23S17::ReadOpcode_v<DataLines>;
            static constexpr auto TargetRegister = static_cast<byte>(isReadOperation ? MCP23S17::Registers::OLAT : MCP23S17::Registers::GPIO);
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
        uint16_t 
        read(const Channel0Value&) const noexcept {
            return line_->getWord(getOffset());
        }
        void
        write(const Channel0Value& m0, uint16_t value) noexcept {
            line_->setWord(getOffset(), value, m0.getByteEnable());
        }
        void
        endTransaction() noexcept override {
            // pull GPIOSelect high
            digitalWrite<Pin::GPIOSelect, HIGH>();
            line_ = nullptr;
        }
    private:
        DataCacheLine* line_;
};

TransactionInterface& 
getCacheInterface() noexcept {
    static CacheOperationHandler handler;
    return handler;
}

#endif

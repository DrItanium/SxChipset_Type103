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

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include "Types.h"
#include "Pinout.h"
#include "MCP23S17.h"
SdFat SD;

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
constexpr auto AddressUpper = MCP23S17::HardwareDeviceAddress::Device1;
constexpr auto AddressLower = MCP23S17::HardwareDeviceAddress::Device2;
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device3;
constexpr auto GPIOA_Lower = MCP23S17::HardwareDeviceAddress::Device4;
constexpr auto GPIOA_Upper = MCP23S17::HardwareDeviceAddress::Device5;
constexpr auto GPIOB_Lower = MCP23S17::HardwareDeviceAddress::Device6;
constexpr auto GPIOB_Upper = MCP23S17::HardwareDeviceAddress::Device7;

[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    pulse<Pin::Ready, LOW, HIGH>();
}
void setSPI1Channel(byte index) noexcept {
    digitalWrite<Pin::SPI2_OFFSET0>(index & 0b001 ? HIGH : LOW);
    digitalWrite<Pin::SPI2_OFFSET1>(index & 0b010 ? HIGH : LOW);
    digitalWrite<Pin::SPI2_OFFSET2>(index & 0b100 ? HIGH : LOW);
}
void setInputChannel(byte value) noexcept {
    if (value & 0b1) {
        digitalWrite<Pin::SEL, HIGH>();
    } else {
        digitalWrite<Pin::SEL, LOW>();
    }
}
void putCPUInReset() noexcept {
    setInputChannel(0);
    digitalWrite<Pin::Reset960, LOW>();
}
void pullCPUOutOfReset() noexcept {
    setInputChannel(0);
    digitalWrite<Pin::Reset960, HIGH>();
}
void configurePins() noexcept;
void setupIOExpanders() noexcept;
void installMemoryImage() noexcept;
void 
setup() {
    Serial.begin(115200);
    SPI.begin();
    // setup the IO Expanders
    MCP23S17::IOCON reg;
    reg.makeInterruptPinsIndependent();
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
    MCP23S17::writeIOCON<AddressLower>(reg);
    MCP23S17::writeDirection<AddressLower>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<AddressUpper>(reg);
    MCP23S17::writeDirection<AddressUpper>(MCP23S17::AllInput16);
    reg.mirrorInterruptPins();
    MCP23S17::writeIOCON<XIO>(reg);
    MCP23S17::writeDirection<XIO>(MCP23S17::AllInput16);
    reg.interruptIsActiveHigh();
    MCP23S17::writeIOCON<GPIOA_Lower>(reg);
    MCP23S17::writeDirection<GPIOA_Lower>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOA_Upper>(reg);
    MCP23S17::writeDirection<GPIOA_Upper>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOB_Lower>(reg);
    MCP23S17::writeDirection<GPIOB_Lower>(MCP23S17::AllInput16);
    MCP23S17::writeIOCON<GPIOB_Upper>(reg);
    MCP23S17::writeDirection<GPIOB_Upper>(MCP23S17::AllInput16);

    // configure pins
    pinMode(Pin::HOLD, OUTPUT);
    pinMode(Pin::HLDA, INPUT);
    pinMode(Pin::GPIOSelect, OUTPUT);
    pinMode(Pin::CS2, OUTPUT);
    pinMode(Pin::SD_EN, OUTPUT);
    pinMode(Pin::PSRAM0, OUTPUT);
    pinMode(Pin::Ready, OUTPUT);
    pinMode(Pin::SPI2_OFFSET0, OUTPUT);
    pinMode(Pin::SPI2_OFFSET1, OUTPUT);
    pinMode(Pin::SPI2_OFFSET2, OUTPUT);
    pinMode(Pin::INT0_, OUTPUT);
    pinMode(Pin::SEL, OUTPUT);
    pinMode(Pin::INT3_, OUTPUT);
    pinMode(Pin::Capture0, INPUT);
    pinMode(Pin::Capture1, INPUT);
    pinMode(Pin::Capture2, INPUT);
    pinMode(Pin::Capture3, INPUT);
    pinMode(Pin::Capture4, INPUT);
    pinMode(Pin::Capture5, INPUT);
    pinMode(Pin::Capture6, INPUT);
    pinMode(Pin::Capture7, INPUT);
    pinMode(Pin::Reset960, OUTPUT);
    setSPI1Channel(0);
    digitalWrite<Pin::Ready, HIGH>();
    digitalWrite<Pin::HOLD, LOW>();
    digitalWrite<Pin::GPIOSelect, HIGH>();
    digitalWrite<Pin::CS2, HIGH>();
    digitalWrite<Pin::INT0_, HIGH>();
    digitalWrite<Pin::INT3_, HIGH>();
    digitalWrite<Pin::PSRAM0, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
    setInputChannel(0);
    putCPUInReset();
    setupCache();
    while (!SD.begin()) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    installMemoryImage();
    pullCPUOutOfReset();
    while (digitalRead<Pin::FAIL>() == LOW) {
        if (digitalRead<Pin::DEN>() == LOW) {
            break;
        }
    }
    while (digitalRead<Pin::FAIL>() == HIGH) {
        if (digitalRead<Pin::DEN>() == LOW) {
            break;
        }
    }
    // okay so we got past this, just start performing actions
}


void handleTransaction() noexcept;
void 
loop() {
    setInputChannel(0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    }
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction();
}

/**
 * @brief Hacked sdCsInit that assumes the only pin we care about is SD_EN, otherwise failure
 * @param pin
 */
void sdCsInit(SdCsPin_t pin) {
    if (static_cast<Pin>(pin) != Pin::SD_EN) {
        Serial.println(F("ERROR! sdCsInit provided sd pin which is not SD_EN"));
        while (true) {
            delay(100);
        }
    } else {
        pinMode(pin, OUTPUT);
    }
}

void sdCsWrite(SdCsPin_t, bool level) {
    digitalWrite<Pin::SD_EN>(level);
}


void 
installMemoryImage() noexcept {

}
void 
doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}

[[gnu::always_inline]] 
inline void 
digitalWrite(Pin pin, decltype(LOW) value) noexcept { 
    if (isPhysicalPin(pin)) {
        if (auto &thePort = getOutputRegister(pin); value == LOW) {
            thePort = thePort & ~getPinMask(pin);
        } else {
            thePort = thePort | getPinMask(pin);
        }
    } else {
        switch (pin) {
            case Pin::SPI2_EN0:
            case Pin::SPI2_EN1:
            case Pin::SPI2_EN2:
            case Pin::SPI2_EN3:
            case Pin::SPI2_EN4:
            case Pin::SPI2_EN5:
            case Pin::SPI2_EN6:
            case Pin::SPI2_EN7:
                digitalWrite(Pin::CS2, value);
                break;
            case Pin::Reset960: 
                doReset(value);
                break;
            default:
                digitalWrite(static_cast<byte>(pin), value); 
                break;
        }
    }
}
[[gnu::always_inline]] 
inline void pinMode(Pin pin, decltype(INPUT) direction) noexcept {
    if (isPhysicalPin(pin)) {
        pinMode(static_cast<int>(pin), direction);
    } else if (pin == Pin::Reset960) {
        auto theDirection = MCP23S17::read8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>();
        if (direction == INPUT || direction == INPUT_PULLUP) {
            theDirection |= 0b1;
        } else if (direction == OUTPUT) {
            theDirection &= ~0b1;
        }
        MCP23S17::write8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>(theDirection);
    }
}
/**
 * @brief Generic fallback handler when an io operation doesn't directly map to * anything
 * @param addr The address/opcode
 * @param m0 The channel0 value contents
 */
void
fallbackIOHandler(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    MCP23S17::writeGPIO16<DataLines>(0);
    for (byte offset = addr.address.offset; offset < 8 /* words per transaction */; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        digitalWrite<Pin::Ready, LOW>();
        digitalWrite<Pin::Ready, HIGH>();
        if (isBurstLast) {
            break;
        }
    }
}
using ReadOperation = uint16_t (*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte);
using WriteOperation = void(*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t);
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0, ReadOperation onRead, WriteOperation onWrite) noexcept {
    for (byte offset = addr.address.offset; offset < 8 /* words per transaction */; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        Channel1Value c1(PINA);
        if (m0.isReadOperation()) {
            MCP23S17::write16<DataLines>(onRead(addr, m0, c1, offset));
        } else {

        }
        digitalWrite<Pin::Ready, LOW>();
        digitalWrite<Pin::Ready, HIGH>();
        if (isBurstLast) {
            break;
        }
    }
}
void
handleSerialOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    switch (addr.ioRequestAddress.function) {
        case 0: // read/write
            break;
        default:
            fallbackIOHandler(addr, m0);
            break;
    }
}

void 
handleIOOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    // When we are in io space, we are treating the address as an opcode which
    // we can decompose while getting the pieces from the io expanders. Thus we
    // can overlay the act of decoding while getting the next part
    // 
    // The W/~R pin is used to figure out if this is a read or write operation
    //
    // This system does not care about the size but it does care about where
    // one starts when performing a write operation
    //IOGroup group = getGroup(addr.bytes[2]);
    switch (addr.getIOGroup()) {
        case IOGroup::Serial:
            handleSerialOperation(addr, m0);
            break;
        default:
            fallbackIOHandler(addr, m0);
            break;
    }
}

void 
handleTransaction() noexcept {
    // grab the entire state of port A
    // update the address as a full 32-bit update for now
    SplitWord32 addr{0};
    Channel0Value m0(PINA);
    setInputChannel(1);
    /// @todo expand this out in the future
    SplitWord16 up(MCP23S17::readGPIO16<AddressUpper>());
    addr.bytes[3] = up.bytes[0];
    addr.bytes[2] = up.bytes[1];
    SplitWord16 down(MCP23S17::readGPIO16<AddressLower>());
    addr.bytes[0] = down.bytes[0];
    addr.bytes[1] = down.bytes[1];
    MCP23S17::writeDirection<DataLines>(m0.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16);
    // interleave operations into the accessing of address lines
    if (addr.isIOInstruction()) {
        handleIOOperation(addr, m0);
    } else {
        // okay now we can service the transaction request since it will be going
        // to ram.
        auto& line = getCache().find(addr);
        auto isReadOp = m0.isReadOperation();
        for (byte offset = addr.address.offset; offset < 8 /* words per transaction */; ++offset) {
            auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
            Channel1Value c1(PINA);
            /// @todo implement
            if (isReadOp) {
                // okay it is a read operation, so... pull a cache line out 
                MCP23S17::writeGPIO16<DataLines>(line.getWord(offset));
            } else {
                // so we are writing to the cache
                line.setWord(offset, MCP23S17::readGPIO16<DataLines>(), c1.bits.be0, c1.bits.be1);
            }
            digitalWrite<Pin::Ready, LOW>();
            digitalWrite<Pin::Ready, HIGH>();
            if (isBurstLast) {
                break;
            }
        }
    }
}

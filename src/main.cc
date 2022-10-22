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
#include <Wire.h>
#include <SdFat.h>
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
void setSPI0Channel(byte index) noexcept {
    digitalWrite<Pin::SPI_OFFSET0>(index & 0b001 ? HIGH : LOW);
    digitalWrite<Pin::SPI_OFFSET1>(index & 0b010 ? HIGH : LOW);
    digitalWrite<Pin::SPI_OFFSET2>(index & 0b100 ? HIGH : LOW);
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
void setupCache() noexcept;
void installMemoryImage() noexcept;
void 
setup() {
    Serial.begin(115200);
    SPI.begin();
    setupIOExpanders();
    configurePins();
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
union Channel0Value {
    explicit Channel0Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t den : 1;
        uint8_t w_r_ : 1;
        uint8_t fail : 1;
        uint8_t unused : 1;
        uint8_t addrInt : 4;
    } bits;
};
union Channel1Value {
    explicit Channel1Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t byteEnable : 2;
        uint8_t blast : 1;
        uint8_t xioint : 1;
        uint8_t dataInt : 2;
        uint8_t ramIO : 1;
        uint8_t unused : 1;
    } bits;
};
template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];

union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    constexpr explicit SplitWord32(uint32_t value) : full(value) { }
    constexpr explicit SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr explicit SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    struct {
        uint32_t a0 : 1;
        uint32_t offset : 3;
        uint32_t rest : 28;
    } address;
    struct {
        static constexpr auto OffsetSize = 4; // 16-byte line
        static constexpr auto TagSize = 7; // 8192 bytes divided into 16-byte
                                           // lines with 4 lines per set
                                           // (4-way)
        static constexpr auto KeySize = 32 - (OffsetSize + TagSize);
        uint32_t offset : OffsetSize;
        uint32_t tag : TagSize; 
        uint32_t key : KeySize;
    } cacheAddress;
};

void 
loop() {
    setInputChannel(0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    }
    while (digitalRead<Pin::DEN>() == HIGH);
    // grab the entire state of port A
    Channel0Value m0(PINA);
    setInputChannel(1);
    // update the address as a full 32-bit update for now
    SplitWord32 addr{0};
    setSPI0Channel(0);
    digitalWrite<Pin::CS1, LOW>();
    SPDR = MCP23S17::generateReadOpcode(AddressUpper);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = 0;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    auto result = SPDR;
    SPDR = 0;
    nop;
    addr.bytes[3] = result;
    while (!(SPSR & _BV(SPIF))) ;
    digitalWrite<Pin::CS1, HIGH>();
    digitalWrite<Pin::CS1, LOW>();
    result = SPDR;
    SPDR = MCP23S17::generateReadOpcode(AddressLower);
    nop;
    addr.bytes[2] = result;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIO);
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = 0;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    result = SPDR;
    SPDR = 0;
    nop;
    addr.bytes[0] = result;
    while (!(SPSR & _BV(SPIF))) ;
    addr.bytes[1] = SPDR;
    digitalWrite<Pin::CS1, HIGH>();

    /// @todo implement optimization to only update this if necessary
    // set data lines direction
    digitalWrite<Pin::CS1, LOW>();
    SPDR = MCP23S17::generateWriteOpcode(DataLines);
    nop;
    // read in this case means output since the i960 is Reading 
    // write in this case means input since the i960 is Writing
    auto lower = m0.bits.w_r_ == 0 ? MCP23S17::AllOutput8 : MCP23S17::AllInput8;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = static_cast<byte>(MCP23S17::Registers::IODIR);
    nop;
    // better utilization of wait states by duplicating behavior for now
    // read in this case means output since the i960 is Reading 
    // write in this case means input since the i960 is Writing
    auto upper = m0.bits.w_r_ == 0 ? MCP23S17::AllOutput8 : MCP23S17::AllInput8;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = lower;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    SPDR = upper;
    nop;
    while (!(SPSR & _BV(SPIF))) ;
    digitalWrite<Pin::CS1, HIGH>();
    // okay now we can service the transaction request 
    
    for (byte offset = addr.address.offset; offset < 8 /* words per transaction */; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        /// @todo insert handler code here
        setSPI0Channel(2);
        digitalWrite<Pin::Ready, LOW>();
        digitalWrite<Pin::Ready, HIGH>();
        if (isBurstLast) {
            break;
        }
    }
}

void 
configurePins() noexcept {
    pinMode(Pin::HOLD, OUTPUT);
    pinMode(Pin::HLDA, INPUT);
    pinMode(Pin::CS1, OUTPUT);
    pinMode(Pin::CS2, OUTPUT);
    pinMode(Pin::SPI_OFFSET0, OUTPUT);
    pinMode(Pin::SPI_OFFSET1, OUTPUT);
    pinMode(Pin::SPI_OFFSET2, OUTPUT);
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
    setSPI0Channel(0);
    setSPI1Channel(0);
    digitalWrite<Pin::HOLD, LOW>();
    digitalWrite<Pin::CS1, HIGH>();
    digitalWrite<Pin::CS2, HIGH>();
    digitalWrite<Pin::INT0_, HIGH>();
    digitalWrite<Pin::INT3_, HIGH>();
    setInputChannel(0);
    putCPUInReset();
}

void
setupIOExpanders() noexcept {

}

/**
 * @brief Hacked sdCsInit that assumes the only pin we care about is SD_EN, otherwise failure
 * @param pin
 */
void sdCsInit(SdCsPin_t pin) {
    /// @todo implement
}

void sdCsWrite(SdCsPin_t, bool level) {
    /// @todo implement
    setSPI0Channel(1);
    digitalWrite<Pin::SD_EN>(level);
}

void setupCache() noexcept {

}

void 
installMemoryImage() noexcept {

}
void 
doReset(decltype(LOW) value) noexcept {
    setSPI0Channel(0);
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::CS1>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::CS1>(theGPIO);
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
            case Pin::Ready:
            case Pin::SD_EN:
            case Pin::SPI1_EN3:
            case Pin::GPIOSelect:
            case Pin::PSRAM0:
            case Pin::PSRAM1:
            case Pin::PSRAM2:
            case Pin::PSRAM3:
                digitalWrite(Pin::CS1, value);
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
        setSPI0Channel(0);
        auto theDirection = MCP23S17::read8<XIO, MCP23S17::Registers::IODIRA, Pin::CS1>();
        if (direction == INPUT || direction == INPUT_PULLUP) {
            theDirection |= 0b1;
        } else if (direction == OUTPUT) {
            theDirection &= ~0b1;
        }
        MCP23S17::write8<XIO, MCP23S17::Registers::IODIRA, Pin::CS1>(theDirection);
    }
}

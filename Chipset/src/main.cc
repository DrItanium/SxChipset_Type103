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
#include "Wire.h"
#include "RTClib.h"
SdFat SD;
// the logging shield I'm using has a DS1307 RTC
RTC_DS1307 rtc;

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
void 
configureReset() noexcept {
    // reset is always an output 
    auto theDirection = MCP23S17::read8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>();
    theDirection &= ~0b1;
    MCP23S17::write8<XIO, MCP23S17::Registers::IODIRA, Pin::GPIOSelect>(theDirection);
}
void
setSPI1Channel(byte index) noexcept {
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
    asm volatile ("nop");
    asm volatile ("nop");
}
constexpr bool EnableDebugMode = false;
constexpr bool EnableInlineSPIOperation = true;
template<bool>
inline void handleTransaction() noexcept;

[[gnu::always_inline]] inline void 
doReset(decltype(LOW) value) noexcept {
    auto theGPIO = MCP23S17::read8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(); 
    if (value == LOW) {
        theGPIO &= ~1;
    } else {
        theGPIO |= 1;
    }
    MCP23S17::write8<XIO, MCP23S17::Registers::OLATA, Pin::GPIOSelect>(theGPIO);
}
void putCPUInReset() noexcept {
    doReset(LOW);
}
void pullCPUOutOfReset() noexcept {
    doReset(HIGH);
}
void configurePins() noexcept;
void setupIOExpanders() noexcept;
void installMemoryImage() noexcept;
uint16_t dataLinesDirection = MCP23S17::AllInput16;
uint16_t currentDataLinesValue = 0;
template<bool performFullMemoryTest>
void
setupPSRAM() noexcept {
    Serial.println(F("RUNNING PSRAM MEMORY TEST!"));
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    digitalWrite<Pin::PSRAM0, LOW>();
    SPI.transfer(0x66);
    digitalWrite<Pin::PSRAM0, HIGH>();
    // test the first 64k instead of the full 8 megabytes
    constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
    for (uint32_t i = 0; i < endAddress; i += 4) {
        union {
            uint32_t whole;
            uint8_t bytes[4];
        } container, result;
        container.whole = i;
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x02); // write
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[3]);
        digitalWrite<Pin::PSRAM0, HIGH>();
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        digitalWrite<Pin::PSRAM0, LOW>();
        SPI.transfer(0x03); // read 
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        result.bytes[0] = SPI.transfer(0);
        result.bytes[1] = SPI.transfer(0);
        result.bytes[2] = SPI.transfer(0);
        result.bytes[3] = SPI.transfer(0);
        digitalWrite<Pin::PSRAM0, HIGH>();
        if (container.whole != result.whole) {
            Serial.print(F("MEMROY TEST FAILURE: W: 0x"));
            Serial.print(container.whole, HEX);
            Serial.print(F(" G: 0x"));
            Serial.println(result.whole, HEX);
            while (true) {
                // halt here
                delay(1000);
            }
        }
    }
    Serial.println(F("MEMORY TEST COMPLETE!"));
}
bool
trySetupDS1307() noexcept {
    if (rtc.begin()) {
        if (!rtc.isrunning()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        return true;
    } else {
        return false;
    }
}
using DateTimeGetter = DateTime(*)();
DateTimeGetter now = nullptr;
void 
setupRTC() noexcept {
    // use short circuiting or to choose the first available rtc
    if (trySetupDS1307()) {
        Serial.println(F("Found RTC DS1307"));
        now = []() { return rtc.now(); };
    } else {
        Serial.println(F("No active RTC found!"));
        now = []() { return DateTime(F(__DATE__), F(__TIME__)); };
    }
}
inline SplitWord32
mspimTransfer4(SplitWord32 value) noexcept {
    SplitWord32 result{0};
    UDR1 = value.bytes[0];
    UDR1 = value.bytes[1];
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[0] = UDR1;
    UDR1 = value.bytes[2];
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[1] = UDR1;
    UDR1 = value.bytes[3];
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[2] = UDR1;
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[3] = UDR1;
    return result;
}
inline SplitWord32
mspimTransfer4(byte a, byte b, byte c, byte d) noexcept {
    SplitWord32 result{0};
    UDR1 = a;
    UDR1 = b;
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[0] = UDR1;
    UDR1 = c;
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[1] = UDR1;
    UDR1 = d;
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[2] = UDR1;
    asm volatile ("nop");
    while (!(UCSR1A & (1 << RXC1)));
    result.bytes[3] = UDR1;
    return result;
}
namespace {
    size_t psram2MemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
    size_t psram2MemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
}
template<bool performFullMemoryTest>
void
setupPSRAM2() noexcept {
    while (!(UCSR1A & (1 << UDRE1)));
    Serial.println(F("RUNNING PSRAM 2 MEMORY TEST!"));
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    for (int j = 0; j < 4; ++j ) {
        setSPI1Channel(j);
        digitalWrite<Pin::CS2, LOW>();
        UDR1 = 0x66;
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1)));
        digitalWrite<Pin::CS2, HIGH>();
    }
    for (int j = 0; j < 4; ++j ) {
        setSPI1Channel(j);
        // test the first 64k instead of the full 8 megabytes
        constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
        for (uint32_t i = 0; i < endAddress; i += 4) {
            SplitWord32 container{i};
#if 0
            digitalWrite<Pin::CS2, LOW>();
            (void)mspimTransfer4(0x02, 
                    container.bytes[2], 
                    container.bytes[1], 
                    container.bytes[0]);
            (void)mspimTransfer4(container.bytes[0], 
                    container.bytes[1], 
                    container.bytes[2],
                    container.bytes[3]);
            digitalWrite<Pin::CS2, HIGH>();
#else
        (void)psram2MemoryWrite(container, container.bytes, sizeof(uint32_t));
#endif

            asm volatile ("nop");
            asm volatile ("nop");
            asm volatile ("nop");
            asm volatile ("nop");
            digitalWrite<Pin::CS2, LOW>();
            (void)mspimTransfer4(0x03, 
                    container.bytes[2], 
                    container.bytes[1], 
                    container.bytes[0]);
            auto result = mspimTransfer4(0, 0, 0, 0);
            digitalWrite<Pin::CS2, HIGH>();
            if (container.full != result.full) {
                Serial.print(F("PSRAM MEMORY 2 TEST FAILURE: W: 0x"));
                Serial.print(container.full, HEX);
                Serial.print(F(" G: 0x"));
                Serial.println(result.full, HEX);
                while (true) {
                    // halt here
                    delay(1000);
                }
            }
        }
    }
    Serial.println(F("PSRAM 2 MEMORY TEST COMPLETE!"));
}
void
setupMSPIM() noexcept {
    pinMode(Pin::CS2, OUTPUT);
    digitalWrite<Pin::CS2, HIGH>();
    UBRR1 = 0x0000; // clear the baud first
    bitSet(DDRD, PD4);
    UCSR1C = 0b11000000; // enable MPSIM and set MSBFIRST and MODE0
    UCSR1B = 0b00011000; // enable transmitter and receiver
    UBRR1 = 0x0001; // set to 5 mhz for testing
}
void 
setup() {
    Serial.begin(115200);
    setupRTC();
    Serial.println(now().unixtime());
    SPI.begin();
    // setup the second spi bus
    setupMSPIM();
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
    MCP23S17::write16<DataLines, MCP23S17::Registers::GPINTEN>(0xFFFF);
    MCP23S17::writeIOCON<AddressLower>(reg);
    MCP23S17::writeDirection<AddressLower>(MCP23S17::AllInput16);
    MCP23S17::write16<AddressLower, MCP23S17::Registers::GPINTEN>(0xFFFF);
    MCP23S17::writeIOCON<AddressUpper>(reg);
    MCP23S17::writeDirection<AddressUpper>(MCP23S17::AllInput16);
    MCP23S17::write16<AddressUpper, MCP23S17::Registers::GPINTEN>(0xFFFF);
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
    dataLinesDirection = MCP23S17::AllInput16;
    currentDataLinesValue = 0;
    MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
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
    configureReset();
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
    while (!SD.begin(static_cast<byte>(Pin::SD_EN))) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    setupPSRAM<false>();
    setupPSRAM2<true>();
    setupCache();
    installMemoryImage();
    // okay so we got the image installed, now we just terminate the SD card
    SD.end();
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
    Serial.println(F("STARTUP COMPLETE! BOOTING..."));
    // okay so we got past this, just start performing actions
    setInputChannel(0);
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<false>();
    setInputChannel(0);
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<false>();
    setInputChannel(0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    } else {
        Serial.println(F("BOOT SUCCESSFUL!"));
    }
}


void 
loop() {
    setInputChannel(0);
    asm volatile ("nop");
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<EnableInlineSPIOperation>();
}

/**
 * @brief Hacked sdCsInit that assumes the only pin we care about is SD_EN, otherwise failure
 * @param pin
 */
void sdCsInit(SdCsPin_t pin) {
    if (static_cast<Pin>(pin) != Pin::SD_EN) {
        Serial.println(F("ERROR! sdCsInit provided sd pin which is not SD_EN"));
        Serial.print(F("Pin is "));
        Serial.println(static_cast<byte>(pin));
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
    if (File memoryImage; !memoryImage.open("boot.sys", FILE_READ)) {
        Serial.println(F("Couldn't open boot.sys!"));
        while (true) {
            delay(1000);
        }
    } else {
        // write out to the data cache as we go along, when we do a miss then
        // we will be successful in writing out to main memory
        memoryImage.seekSet(0);
        Serial.println(F("installing memory image from sd"));
        constexpr auto BufferSize = 1024;
        uint8_t buffer[BufferSize];
        for (uint32_t i = 0, j = 0; i < memoryImage.size(); i += BufferSize, ++j) {
            while (memoryImage.isBusy());
            SplitWord32 currentAddressLine(i);
            auto numRead = memoryImage.read(buffer, BufferSize);
            if (numRead < 0) {
                SD.errorHalt();
            }
            memoryWrite(currentAddressLine, buffer, BufferSize);
            if ((j % 16) == 0) {
                Serial.print(F("."));
            }
        }
        memoryImage.close();
        Serial.println();
        Serial.println(F("transfer complete!"));
    }
}


template<bool busHeldOpen>
inline void 
setDataLinesOutput(uint16_t value) noexcept {
    if (currentDataLinesValue != value) {
        currentDataLinesValue = value;
        if constexpr (busHeldOpen) {
            SPDR = static_cast<byte>(value);
            asm volatile ("nop");
            auto next = static_cast<byte>(value >> 8);
            while (!(SPSR & _BV(SPIF))) ; // wait
            SPDR = next;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
        } else {
            MCP23S17::write16<DataLines, MCP23S17::Registers::OLAT>(currentDataLinesValue);
        }
    }
}
SplitWord16 previousValue{0};
template<bool busHeldOpen>
inline uint16_t 
getDataLines(const Channel1Value& c1) noexcept {
    if (c1.channel1.dataInt != 0b11) {
        if constexpr (busHeldOpen) {
            SPDR = 0;
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
            auto value = SPDR;
            SPDR = 0;
            asm volatile ("nop");
            previousValue.bytes[0] = value;
            while (!(SPSR & _BV(SPIF))) ; // wait
            previousValue.bytes[1] = SPDR;
        } else {
            previousValue.full = MCP23S17::readGPIO16<DataLines>();
        }
    }
    return previousValue.full;
}
using ReadOperation = uint16_t (*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte);
using WriteOperation = void(*)(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t);
template<bool isReadOperation>
void
genericIOHandler(const SplitWord32& addr, const Channel0Value& m0, ReadOperation onRead, WriteOperation onWrite) noexcept {
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        Channel1Value c1(PINA);
        if constexpr (isReadOperation) {
            setDataLinesOutput<false>(onRead(addr, m0, c1, offset));
        } else {
            onWrite(addr, m0, c1, offset, getDataLines<false>(c1));
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
}
uint16_t
performSerialRead_Fast(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    return Serial.read();
}

uint16_t
performSerialRead_Compact(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    uint16_t output = 0xFFFF;
    (void)Serial.readBytes(reinterpret_cast<byte*>(&output), sizeof(output));
    return output;
}
void
performSerialWrite_Fast(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t value) noexcept {
    Serial.write(static_cast<uint8_t>(value));
}

void
performSerialWrite_Compact(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t value) noexcept {
    Serial.write(reinterpret_cast<byte*>(&value), sizeof(value));
}
void
performNullWrite(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte, uint16_t) noexcept {
}
uint16_t
performNullRead(const SplitWord32&, const Channel0Value&, const Channel1Value&, byte) noexcept {
    return 0;
}
template<bool isReadOperation>
inline void
handleSerialOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    switch (addr.getIOFunction<SerialGroupFunction>()) {
        case SerialGroupFunction::RWFast:
            genericIOHandler<isReadOperation>(addr, m0, performSerialRead_Fast, performSerialWrite_Fast);
            break;
        case SerialGroupFunction::RWCompact:
            genericIOHandler<isReadOperation>(addr, m0, performSerialRead_Compact, performSerialWrite_Compact);
            break;
        case SerialGroupFunction::Flush:
            Serial.flush();
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}
template<MCP23S17::HardwareDeviceAddress addr, MCP23S17::Registers reg>
void
performGPIOPortWrite(const SplitWord32& offset, const Channel0Value&, const Channel1Value&, byte, uint16_t) noexcept {
}
template<MCP23S17::HardwareDeviceAddress addr, MCP23S17::Registers reg>
uint16_t
performGPIOPortRead(const SplitWord32& offset, const Channel0Value&, const Channel1Value&, byte) noexcept {
    return 0;
}

template<bool isReadOperation>
inline void
handleGPIOAOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    /*
    IODIR,
    IPOL,
    GPINTEN,
    DEFVAL,
    INTCON,
    IOCON,
    GPPU,
    INTF,
    INTCAP,
    GPIO,
    OLAT,
    */
    switch (addr.getIOFunction<GPIOFunction>()) {
        case GPIOFunction::IODIR:
        case GPIOFunction::IPOL:
        case GPIOFunction::GPINTEN:
        case GPIOFunction::DEFVAL:
        case GPIOFunction::INTCON:
        case GPIOFunction::IOCON:
        case GPIOFunction::GPPU:
        case GPIOFunction::INTF:
        case GPIOFunction::INTCAP:
        case GPIOFunction::GPIO:
        case GPIOFunction::OLAT:
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}
template<bool isReadOperation>
inline void
handleGPIOBOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    switch (addr.getIOFunction<GPIOFunction>()) {
        case GPIOFunction::IODIR:
        case GPIOFunction::IPOL:
        case GPIOFunction::GPINTEN:
        case GPIOFunction::DEFVAL:
        case GPIOFunction::INTCON:
        case GPIOFunction::IOCON:
        case GPIOFunction::GPPU:
        case GPIOFunction::INTF:
        case GPIOFunction::INTCAP:
        case GPIOFunction::GPIO:
        case GPIOFunction::OLAT:
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}

template<bool isReadOperation>
inline void 
handleIOOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    // When we are in io space, we are treating the address as an opcode which
    // we can decompose while getting the pieces from the io expanders. Thus we
    // can overlay the act of decoding while getting the next part
    // 
    // The W/~R pin is used to figure out if this is a read or write operation
    //
    // This system does not care about the size but it does care about where
    // one starts when performing a write operation
    switch (addr.getIOGroup()) {
        case IOGroup::Serial:
            handleSerialOperation<isReadOperation>(addr, m0);
            break;
        case IOGroup::GPIOA:
            handleGPIOAOperation<isReadOperation>(addr, m0);
            break;
        case IOGroup::GPIOB:
            handleGPIOBOperation<isReadOperation>(addr, m0);
            break;
        default:
            genericIOHandler<isReadOperation>(addr, m0, performNullRead, performNullWrite);
            break;
    }
}
template<bool isReadOperation, bool inlineSPIOperation>
void
handleCacheOperation(const SplitWord32& addr, const Channel0Value& m0) noexcept {
    // okay now we can service the transaction request since it will be going
    // to ram.
    auto& line = getCache().find(addr);
    if constexpr (inlineSPIOperation) {
        digitalWrite<Pin::GPIOSelect, LOW>();
        static constexpr auto TargetAction = isReadOperation ? MCP23S17::WriteOpcode_v<DataLines> : MCP23S17::ReadOpcode_v<DataLines>;
        static constexpr auto TargetRegister = static_cast<byte>(isReadOperation ? MCP23S17::Registers::OLAT : MCP23S17::Registers::GPIO);
        SPDR = TargetAction;
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))); 
        SPDR = TargetRegister;
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))); 
    }
    for (byte offset = addr.address.offset; ; ++offset) {
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        if constexpr (isReadOperation) {
            // okay it is a read operation, so... pull a cache line out 
            auto value = line.getWord(offset);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tGot Value: 0x"));
                Serial.println(value, HEX);
            }
            setDataLinesOutput<inlineSPIOperation>(value);
        } else {
            Channel1Value c1(PINA);
            auto value = getDataLines<inlineSPIOperation>(c1);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            // so we are writing to the cache
            line.setWord(offset, value, c1.getByteEnable());
        }
        signalReady();
        if (isBurstLast) {
            break;
        }
    }
    if constexpr (inlineSPIOperation) {
        digitalWrite<Pin::GPIOSelect, HIGH>();
    }
}
enum class TransactionKind {
    // 0b00 -> cache + read
    // 0b01 -> cache + write
    // 0b10 -> io + read
    // 0b11 -> io + write
    CacheRead,
    CacheWrite,
    IORead,
    IOWrite,
};

inline TransactionKind getTransaction(Channel0Value m0, const SplitWord32& addr) noexcept {
    if (addr.isIOInstruction()) {
        if (m0.isReadOperation()) {
            return TransactionKind::IORead;
        } else {
            return TransactionKind::IOWrite;
        }
    } else {
        if (m0.isReadOperation()) {
            return TransactionKind::CacheRead;
        } else {
            return TransactionKind::CacheWrite;
        }
    }
}
template<bool EnableInlineSPIOperation>
inline void 
handleTransaction() noexcept {
    static SplitWord32 addr{0};
    uint16_t direction = 0;
    uint8_t value = 0;
    bool updateDataLines = false;
    TransactionKind target = TransactionKind::CacheRead;
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    // grab the entire state of port A
    // update the address as a full 32-bit update for now
    Channel0Value m0(PINA);
    if (m0.channel0.upperAddr != 0b11) {
        digitalWrite<Pin::GPIOSelect, LOW>();
        SPDR = MCP23S17::ReadOpcode_v<AddressUpper>;
        asm volatile("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(MCP23S17::Registers::GPIO) ;
        asm volatile("nop");
        setInputChannel(1);
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = 0;
        asm volatile("nop");
        direction = m0.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
        while (!(SPSR & _BV(SPIF))) ; // wait
        value = SPDR;
        SPDR = 0;
        asm volatile("nop");
        addr.bytes[3] = value;
        while (!(SPSR & _BV(SPIF))) ; // wait
        value = SPDR;
        digitalWrite<Pin::GPIOSelect, HIGH>();
    } else {
        setInputChannel(1);
        direction = m0.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    }

    if (m0.channel0.lowerAddr != 0b11) {
        digitalWrite<Pin::GPIOSelect, LOW>();
        SPDR = MCP23S17::ReadOpcode_v<AddressLower>;
        asm volatile("nop");
        if (m0.channel0.upperAddr != 0b11) {
            addr.bytes[2] = value;
        }
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = static_cast<byte>(MCP23S17::Registers::GPIO) ;
        asm volatile("nop");
        updateDataLines = direction != dataLinesDirection;
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = 0;
        asm volatile("nop");
        target = getTransaction(m0, addr);
        while (!(SPSR & _BV(SPIF))) ; // wait
        value = SPDR;
        SPDR = 0;
        asm volatile("nop");
        addr.bytes[0] = value;
        while (!(SPSR & _BV(SPIF))) ; // wait
        addr.bytes[1] = SPDR;
        digitalWrite<Pin::GPIOSelect, HIGH>();
    } else {
        if (m0.channel0.upperAddr != 0b11) {
            addr.bytes[2] = value;
        }
        updateDataLines = direction != dataLinesDirection;
        target = getTransaction(m0, addr);
    }
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target address: 0x"));
        Serial.println(addr.getWholeValue(), HEX);
        Serial.print(F("Operation: "));
        if (m0.isReadOperation()) {
            Serial.println(F("Read!"));
        } else {
            Serial.println(F("Write!"));
        }
    }
    if (updateDataLines) {
        dataLinesDirection = direction;
        MCP23S17::writeDirection<DataLines>(dataLinesDirection);
    }
    switch (target) {
        case TransactionKind::CacheRead:
            handleCacheOperation<true, EnableInlineSPIOperation>(addr, m0);
            break;
        case TransactionKind::CacheWrite:
            handleCacheOperation<false, EnableInlineSPIOperation>(addr, m0);
            break;
        case TransactionKind::IORead:
            handleIOOperation<true>(addr, m0);
            break;
        case TransactionKind::IOWrite:
            handleIOOperation<false>(addr, m0);
            break;
    }
    SPI.endTransaction();
}


namespace {
    size_t
    psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
        SPDR = 0x02;
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[2];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[1];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[0];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        for (size_t i = 0; i < count; ++i) {
            SPDR = bytes[i]; 
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
        }
        digitalWrite<Pin::PSRAM0, HIGH>();
        return count;
    }
    size_t
    psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
        SPDR = 0x03;
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[2];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[1];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        SPDR = baseAddress.bytes[0];
        asm volatile ("nop");
        while (!(SPSR & _BV(SPIF))) ; // wait
        for (size_t i = 0; i < count; ++i) {
            SPDR = 0; 
            asm volatile ("nop");
            while (!(SPSR & _BV(SPIF))) ; // wait
            bytes[i] = SPDR;
        }
        digitalWrite<Pin::PSRAM0, HIGH>();
        return count;
    }
    /// @todo implement psram2 support
    size_t
    psram2MemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        volatile byte sink = 0;
        setSPI1Channel(baseAddress.psram2Address.key);
        digitalWrite<Pin::CS2, LOW>();
        UDR1 = 0x02;
        UDR1 = baseAddress.psram2Address.higher;
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1))); // 0x02 transferred
        sink = UDR1;
        UDR1 = baseAddress.psram2Address.middle;
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1))); // highest 7 bits transferred
        sink = UDR1;
        UDR1 = baseAddress.psram2Address.lowest;
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1))); // middle 8 bits transferred
        // so at this point we have some choices, first is to 
        sink = UDR1;
        UDR1 = bytes[0];
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1))); // lowest 8 bits transferred
        for (size_t i = 1; i < count; ++i) {
            sink = UDR1;
            UDR1 = bytes[i];
            asm volatile ("nop");
            while (!(UCSR1A & (1 << RXC1))); // first byte/next byte 
        }
        sink = UDR1;
        asm volatile ("nop");
        while (!(UCSR1A & (1 << RXC1)));
        sink = UDR1;
        digitalWrite<Pin::CS2, HIGH>();
        return count;
    }
}
size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead(baseAddress, bytes, count);
}

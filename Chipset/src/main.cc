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
#include "Adafruit_RGBLCDShield.h"
#include "Peripheral.h"

SdFat SD;
// the logging shield I'm using has a DS1307 RTC
RTC_DS1307 rtc;
SerialDevice theSerial;
InfoDevice infoDevice;
Adafruit_RGBLCDShield lcd;
void 
setInputChannel(byte value) noexcept {
    switch(value & 0b11) {
        case 0b00:
            digitalWrite<Pin::SEL, LOW>();
            digitalWrite<Pin::SEL1, LOW>();
            break;
        case 0b01:
            digitalWrite<Pin::SEL, HIGH>();
            digitalWrite<Pin::SEL1, LOW>();
            break;
        case 0b10:
            digitalWrite<Pin::SEL, LOW>();
            digitalWrite<Pin::SEL1, HIGH>();
            break;
        case 0b11:
            digitalWrite<Pin::SEL, HIGH>();
            digitalWrite<Pin::SEL1, HIGH>();
            break;
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
putCPUInReset() noexcept {
    doReset(LOW);
}
void 
pullCPUOutOfReset() noexcept {
    doReset(HIGH);
}
void 
holdBus() noexcept {
    doHold(HIGH);
} 

void 
releaseBus() noexcept {
    doHold(LOW);
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
            Serial.print(F("MEMORY TEST FAILURE: W: 0x"));
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
enum class RGBLCDColors : byte {
    Red = 0x1,
    Green,
    Yellow,
    Blue,
    Violet,
    Teal,
    White,
};
void
setupRGBLCDShield() noexcept {
    Serial.print(F("Bringing up lcd shield..."));
    lcd.begin(16, 2);
    lcd.setBacklight(static_cast<byte>(RGBLCDColors::White));
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print(F("BOOTING!"));
    Serial.println(F("DONE!"));
}
void
setupIOExpanders() noexcept {
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
    reg.mirrorInterruptPins();
    reg.interruptIsActiveHigh();
    MCP23S17::writeIOCON<XIO>(reg);
    MCP23S17::writeDirection<XIO>(0, 0xFF);
    MCP23S17::writeGPIO8_PORTA<XIO>(0); // don't hold the bus but put the CPU
                                        // into reset
    MCP23S17::write16<XIO, MCP23S17::Registers::GPINTEN>(0xFF00);

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
    pinMode<Pin::INT0_>(OUTPUT);
    pinMode<Pin::SEL>(OUTPUT);
    pinMode<Pin::SEL1>(OUTPUT);
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
    digitalWrite<Pin::INT0_, HIGH>();
    digitalWrite<Pin::PSRAM0, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
    setInputChannel(0);
}
void
bootCPU() noexcept {
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
    lcd.clear();
    lcd.setCursor(0,0);
    if (digitalRead<Pin::FAIL>() == HIGH) {
        lcd.println(F("CHECKSUM FAILURE!"));
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true);
    } else {
        lcd.print(F("BOOT SUCCESSFUL!"));
        Serial.println(F("BOOT SUCCESSFUL!"));
    }
}
void 
setup() {
    theSerial.begin();
    infoDevice.begin();
    Wire.begin();
    setupRGBLCDShield();
    setupRTC();
    SPI.begin();
    // setup the IO Expanders
    setupIOExpanders();
    configurePins();
    while (!SD.begin(static_cast<byte>(Pin::SD_EN))) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    setupPSRAM<false>();
    setupCache();
    installMemoryImage();
    // okay so we got the image installed, now we just terminate the SD card
    SD.end();
    bootCPU();
}


void 
loop() {
    setInputChannel(0);
    asm volatile ("nop");
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<EnableInlineSPIOperation>();
}

void sdCsInit(SdCsPin_t pin) {
    pinMode(pin, OUTPUT);
}

void sdCsWrite(SdCsPin_t pin, bool level) {
    digitalWrite(pin, level);
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
        auto BufferSize = getCache().sizeOfBuffer();
        auto* buffer = getCache().asBuffer();
        for (uint32_t i = 0, j = 0; i < memoryImage.size(); i += BufferSize, ++j) {
            while (memoryImage.isBusy());
            SplitWord32 currentAddressLine(i);
            auto numRead = memoryImage.read(buffer, BufferSize);
            if (numRead < 0) {
                SD.errorHalt();
            }
            memoryWrite(currentAddressLine, buffer, numRead);
            if ((j % 16) == 0) {
                Serial.print(F("."));
            }
        }
        memoryImage.close();
        Serial.println();
        Serial.println(F("transfer complete!"));
        getCache().clear();
    }
}

SplitWord16 previousValue{0};


template<bool isReadOperation>
inline void 
handlePeripheralOperation(const SplitWord32& addr) noexcept {
    switch (addr.getIODevice<TargetPeripheral>()) {
        case TargetPeripheral::Info:
            infoDevice.handleExecution<isReadOperation>(addr);
            break;
        case TargetPeripheral::Serial:
            theSerial.handleExecution<isReadOperation>(addr);
            break;
        default:
            genericIOHandler<isReadOperation>(addr);
            break;
    }
}

template<bool isReadOperation>
inline void 
handleIOOperation(const SplitWord32& addr) noexcept {
    // When we are in io space, we are treating the address as an opcode which
    // we can decompose while getting the pieces from the io expanders. Thus we
    // can overlay the act of decoding while getting the next part
    // 
    // The W/~R pin is used to figure out if this is a read or write operation
    //
    // This system does not care about the size but it does care about where
    // one starts when performing a write operation
    switch (addr.getIOGroup()) {
        case IOGroup::Peripherals:
            handlePeripheralOperation<isReadOperation>(addr);
            break;
        default:
            genericIOHandler<isReadOperation>(addr);
            break;
    }
}
template<bool isReadOperation, bool inlineSPIOperation>
void
handleCacheOperation(const SplitWord32& addr) noexcept {
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
            auto c0 = readInputChannelAs<Channel0Value>();
            auto value = getDataLines<inlineSPIOperation>(c0);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            // so we are writing to the cache
            line.setWord(offset, value, c0.getByteEnable());
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

template<bool EnableInlineSPIOperation>
inline void 
handleTransaction() noexcept {
    static SplitWord32 addr{0};
    uint16_t direction = 0;
    bool updateDataLines = false;
    TransactionKind target = TransactionKind::CacheRead;
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    // grab the entire state of port A
    // update the address as a full 32-bit update for now
    digitalWrite<Pin::GPIOSelect, LOW>();
    SPDR = MCP23S17::ReadOpcode_v<XIO>;
    asm volatile("nop");
    setInputChannel(1);
    addr.bytes[2] = readInputChannelAs<Channel1Value>().getAddressBits16_23();
    while (!(SPSR & _BV(SPIF))) ; // wait
    SPDR = static_cast<byte>(MCP23S17::Registers::GPIOB) ;
    asm volatile("nop");
    setInputChannel(2);
    auto m2 = readInputChannelAs<Channel2Value>();
    direction = m2.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    addr.bytes[0] = m2.getAddressBits0_7();
    while (!(SPSR & _BV(SPIF))) ; // wait
    SPDR = 0;
    asm volatile("nop");
    setInputChannel(3);
    addr.bytes[1] = readInputChannelAs<Channel3Value>().getAddressBits8_15();
    setInputChannel(0);
    updateDataLines = direction != dataLinesDirection;
    while (!(SPSR & _BV(SPIF))) ; // wait
    addr.bytes[3] = SPDR;
    if (addr.isIOInstruction()) {
        if (m2.isReadOperation()) {
            target = TransactionKind::IORead;
        } else {
            target = TransactionKind::IOWrite;
        }
    } else {
        if (m2.isReadOperation()) {
            target = TransactionKind::CacheRead;
        } else {
            target = TransactionKind::CacheWrite;
        }
    }
    digitalWrite<Pin::GPIOSelect, HIGH>();
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target address: 0x"));
        Serial.println(addr.getWholeValue(), HEX);
        Serial.print(F("Operation: "));
        if (m2.isReadOperation()) {
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
            handleCacheOperation<true, EnableInlineSPIOperation>(addr);
            break;
        case TransactionKind::CacheWrite:
            handleCacheOperation<false, EnableInlineSPIOperation>(addr);
            break;
        case TransactionKind::IORead:
            handleIOOperation<true>(addr);
            break;
        case TransactionKind::IOWrite:
            handleIOOperation<false>(addr);
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
}
size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead(baseAddress, bytes, count);
}




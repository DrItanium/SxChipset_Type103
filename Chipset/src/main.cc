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
#include "Peripheral.h"
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
#include "Adafruit_RGBLCDShield.h"
#endif

SdFat SD;
// the logging shield I'm using has a DS1307 RTC
RTC_DS1307 rtc;
SerialDevice theSerial;
InfoDevice infoDevice;
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
Adafruit_RGBLCDShield lcd;
#endif
constexpr bool EnableDebugMode = false;
constexpr bool EnableTimingDebug = false;
constexpr bool EnableInlineSPIOperation = true;

template<bool doDebugCheck = EnableTimingDebug>
[[gnu::always_inline]] inline 
void 
singleCycleDelay() noexcept {
    if constexpr (doDebugCheck) {
        delay(1);
    } else {
        asm volatile ("nop");
        asm volatile ("nop");
    }
}

template<bool, bool DisableInterruptChecks = true>
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
void configurePins() noexcept;
void setupIOExpanders() noexcept;
void installMemoryImage() noexcept;
uint16_t dataLinesDirection = MCP23S17::AllInput16;
uint16_t currentDataLinesValue = 0;
template<bool performFullMemoryTest>
void
setupPSRAM() noexcept {
    SPI.beginTransaction(SPISettings(F_CPU/2, MSBFIRST, SPI_MODE0));
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
    SPI.endTransaction();
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
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
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
#endif
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
    MCP23S17::writeDirection<XIO>(0b1111'1100, 0b1111'1111);
    MCP23S17::writeGPIO8_PORTA<XIO>(0); // don't hold the bus but put the CPU
                                        // into reset
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
    pinMode<Pin::INT0_>(OUTPUT);
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
    digitalWrite<Pin::INT0_, HIGH>();
    digitalWrite<Pin::PSRAM0, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
    digitalWrite<Pin::Enable, HIGH>();
    // do an initial clear of the clock signal
    pulse<Pin::CLKSignal, LOW, HIGH>();
}
void
bootCPU() noexcept {
    pullCPUOutOfReset();
    bool skippedAhead = false;
    while (digitalRead<Pin::FAIL>() == LOW) {
        if (digitalRead<Pin::DEN>() == LOW) {
            skippedAhead = true;
            break;
        }
    }
    while (digitalRead<Pin::FAIL>() == HIGH) {
        if (digitalRead<Pin::DEN>() == LOW) {
            skippedAhead = true;
            break;
        }
    }
    Serial.println(F("STARTUP COMPLETE! BOOTING..."));
    if (skippedAhead) {
        Serial.println(F("\t Skipped ahead!"));
    }
    // okay so we got past this, just start performing actions
    //setInputChannel<0>();
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<false, true>();
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<false, true>();
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
    lcd.clear();
    lcd.setCursor(0,0);
#endif
    if (digitalRead<Pin::FAIL>() == HIGH) {
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
        lcd.println(F("CHECKSUM FAILURE!"));
#endif
        Serial.println(F("CHECKSUM FAILURE!"));
        while (true) {
            delay(1000);
        }
    } else {
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
        lcd.print(F("BOOT SUCCESSFUL!"));
#endif
        Serial.println(F("BOOT SUCCESSFUL!"));
    }
}
void 
setup() {
    theSerial.begin();
    infoDevice.begin();
    Wire.begin();
#ifdef SUPPORT_ADAFRUIT_RGBLCDSHIELD
    setupRGBLCDShield();
#endif
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
    singleCycleDelay();
    singleCycleDelay();
    while (digitalRead<Pin::DEN>() == HIGH);
    handleTransaction<EnableInlineSPIOperation, false>();
    // always make sure we wait enough time
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
template<bool isReadOperation, bool inlineSPIOperation, bool disableWriteInterrupt>
void
handleCacheOperation(const SplitWord32& addr) noexcept {
    // okay now we can service the transaction request since it will be going
    // to ram.
    auto& line = getCache().find(addr);
    if constexpr (inlineSPIOperation) {
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
    for (byte offset = addr.getAddressOffset(); ; ++offset) {
        auto c0 = readInputChannelAs<Channel0Value, true>();
        if constexpr (EnableDebugMode) {
            Serial.print(F("\tOffset: 0x"));
            Serial.println(offset, HEX);
            Serial.print(F("\tChannel0: 0b"));
            Serial.println(static_cast<int>(c0.getWholeValue()), BIN);
        }
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
            auto value = getDataLines<inlineSPIOperation, disableWriteInterrupt>(c0);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            // so we are writing to the cache
            line.setWord(offset, value, c0.getByteEnable());
        }
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
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

[[gnu::always_inline]] 
inline void 
triggerClock() noexcept {
    pulse<Pin::CLKSignal, LOW, HIGH>();
    singleCycleDelay();
    singleCycleDelay();
}
template<bool EnableInlineSPIOperation, bool DisableInterruptChecks = true>
inline void 
handleTransaction() noexcept {
    SplitWord32 addr { 0 };
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    triggerClock();
    digitalWrite<Pin::Enable, LOW>();
    singleCycleDelay();
    auto m2 = readInputChannelAs<Channel2Value>();
    addr.bytes[0] = m2.getWholeValue();
    addr.address.a0 = 0;
    triggerClock();
    addr.bytes[1] = readInputChannelAs<Channel3Value>().getAddressBits8_15();
    triggerClock();
    addr.bytes[2] = readInputChannelAs<Channel1Value>().getAddressBits16_23();
    triggerClock();
    addr.bytes[3] = readInputChannelAs<Word8>().getWholeValue();
    digitalWrite<Pin::Enable, HIGH>();
    triggerClock();
    auto direction = m2.isReadOperation() ? MCP23S17::AllOutput16 : MCP23S17::AllInput16;
    auto updateDataLines = direction != dataLinesDirection;
    TransactionKind target;
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
    if (updateDataLines) {
        dataLinesDirection = direction;
        MCP23S17::writeDirection<DataLines>(dataLinesDirection);
    }

    if constexpr (EnableDebugMode) {
        Serial.print(F("Target address: 0x"));
        Serial.print(addr.getWholeValue(), HEX);
        Serial.print(F("(0b"));
        Serial.print(addr.getWholeValue(), BIN);
        Serial.println(F(")"));
        Serial.print(F("Operation: "));
        if (m2.isReadOperation()) {
            Serial.println(F("Read!"));
        } else {
            Serial.println(F("Write!"));
        }
    }
    switch (target) {
        case TransactionKind::CacheRead:
            handleCacheOperation<true, EnableInlineSPIOperation, DisableInterruptChecks>(addr);
            break;
        case TransactionKind::CacheWrite:
            handleCacheOperation<false, EnableInlineSPIOperation, DisableInterruptChecks>(addr);
            break;
        case TransactionKind::IORead:
            handleIOOperation<true>(addr);
            break;
        case TransactionKind::IOWrite:
            handleIOOperation<false>(addr);
            break;
    }
    SPI.endTransaction();
    // allow for extra recovery time, introduce a single 10mhz cycle delay
    // shift back to input channel 0
    singleCycleDelay();
}


namespace {
    size_t
    psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
#ifdef AVR_SPI_AVAILABLE
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
#else
        SPI.transfer(0x02);
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
#endif
        digitalWrite<Pin::PSRAM0, HIGH>();
        return count;
    }
    size_t
    psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<Pin::PSRAM0, LOW>();
#ifdef AVR_SPI_AVAILABLE
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
#else
        SPI.transfer(0x03);
        SPI.transfer(baseAddress.bytes[2]);
        SPI.transfer(baseAddress.bytes[1]);
        SPI.transfer(baseAddress.bytes[0]);
        SPI.transfer(bytes, count);
#endif
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


// if the AVR processor doesn't have access to the GPIOR registers then emulate
// them
#ifndef GPIOR0
byte GPIOR0;
#endif
#ifndef GPIOR1
byte GPIOR1;
#endif
#ifndef GPIOR2
byte GPIOR2;
#endif

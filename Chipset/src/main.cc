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
#include "Wire.h"
#include "RTClib.h"
#include "Peripheral.h"
#include "Setup.h"

SdFat SD;
// the logging shield I'm using has a DS1307 RTC
SerialDevice theSerial;
InfoDevice infoDevice;
TimerDevice timerInterface;


void handleTransaction() noexcept;

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
template<Pin targetPin, bool performFullMemoryTest>
bool
setupPSRAM() noexcept {
    SPI.beginTransaction(SPISettings(F_CPU/2, MSBFIRST, SPI_MODE0));
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    digitalWrite<targetPin, LOW>();
    SPI.transfer(0x66);
    digitalWrite<targetPin, HIGH>();
    // test the first 64k instead of the full 8 megabytes
    constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
    for (uint32_t i = 0; i < endAddress; i += 4) {
        SplitWord32 container {i}, result {0};
        digitalWrite<targetPin, LOW>();
        SPI.transfer(0x02); // write
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[3]);
        digitalWrite<targetPin, HIGH>();
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        digitalWrite<targetPin, LOW>();
        SPI.transfer(0x03); // read 
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        result.bytes[0] = SPI.transfer(0);
        result.bytes[1] = SPI.transfer(0);
        result.bytes[2] = SPI.transfer(0);
        result.bytes[3] = SPI.transfer(0);
        digitalWrite<targetPin, HIGH>();
        if (container != result) {
            return false;
        }
    }
    SPI.endTransaction();
    return true;
}
template<bool performFullMemoryTest>
void
queryPSRAM() noexcept {
    uint32_t memoryAmount = 0;
    auto addPSRAMAmount = [&memoryAmount]() {
        memoryAmount += (8ul * 1024ul * 1024ul);
    };
    if (setupPSRAM<Pin::PSRAM0, performFullMemoryTest>()) {
        addPSRAMAmount();
    }
    if (memoryAmount == 0) {
        Serial.println(F("NO MEMORY INSTALLED!"));
        while (true) {
            delay(1000);
        }
    } else {
        Serial.print(F("Detected "));
        Serial.print(memoryAmount);
        Serial.println(F(" bytes of memory!"));
    }
}
bool
trySetupDS1307() noexcept {
    return timerInterface.begin();
}
void 
setupRTC() noexcept {
    // use short circuiting or to choose the first available rtc
    if (trySetupDS1307()) {
        Serial.println(F("Found RTC DS1307"));
    } else {
        Serial.println(F("No active RTC found!"));
    }
}
[[gnu::always_inline]]
inline void 
waitForDataState() noexcept {
    singleCycleDelay();
    while (digitalRead<Pin::DEN>() == HIGH);
}
[[gnu::always_inline]] inline void 
handleTransactionCycle() noexcept {
    waitForDataState();
    handleTransaction();
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
    handleTransactionCycle();
    handleTransactionCycle();
    if (digitalRead<Pin::FAIL>() == HIGH) {
        Serial.println(F("CHECKSUM FAILURE!"));
    } else {
        Serial.println(F("BOOT SUCCESSFUL!"));
    }
}
void
testCoprocessor() noexcept {
    Wire.beginTransmission(8);
    Wire.write("Donuts are tasty!");
    Wire.write(32);
    Wire.endTransmission();
}
void 
setup() {
    theSerial.begin();
    infoDevice.begin();
    Wire.begin();
    testCoprocessor();
    setupRTC();
    SPI.begin();
    // setup the IO Expanders
    setupAddressAndDataLines();
    while (!SD.begin(static_cast<byte>(Pin::SD_EN))) {
        Serial.println(F("NO SD CARD FOUND...WAITING!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    queryPSRAM<false>();
    setupCache();
    installMemoryImage();
    // okay so we got the image installed, now we just terminate the SD card
    SD.end();
    bootCPU();
}


void 
loop() {
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    for (;;) {
        handleTransactionCycle();
    }
    SPI.endTransaction();
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

inline void runFallbackHandler(const SplitWord32& addr, OperationHandlerUser fn) noexcept {
   fn(addr, getNullHandler()); 
}
inline void 
handlePeripheralOperation(const SplitWord32& addr, OperationHandlerUser fn) noexcept {
    switch (addr.getIODevice<TargetPeripheral>()) {
        case TargetPeripheral::Info:
            fn(addr, infoDevice);
            break;
        case TargetPeripheral::Serial:
            fn(addr, theSerial);
            break;
        case TargetPeripheral::RTC:
            fn(addr, timerInterface);
            break;
        default:
            runFallbackHandler(addr, fn);
            break;
    }
}

inline void 
handleIOOperation(const SplitWord32& addr, OperationHandlerUser fn) noexcept {
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
            handlePeripheralOperation(addr, fn);
            break;
        default:
            runFallbackHandler(addr, fn);
            break;
    }
}

inline void 
handleTransaction() noexcept {
    enterTransactionSetup();
    auto addr = configureTransaction();
    leaveTransactionSetup();
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target address: 0x"));
        Serial.print(addr.getWholeValue(), HEX);
        Serial.print(F("(0b"));
        Serial.print(addr.getWholeValue(), BIN);
        Serial.println(F(")"));
        Serial.print(F("Operation: "));
        if (isReadOperation()) {
            Serial.println(F("Read!"));
        } else {
            Serial.println(F("Write!"));
        }
    }
    if (auto fn = getFunction(addr); addr.isIOInstruction()) {
        handleIOOperation(addr, fn);
    } else {
        fn(addr, getCacheInterface());
    }
    // allow for extra recovery time, introduce a single 10mhz cycle delay
    // shift back to input channel 0
    singleCycleDelay();
}


namespace {
    template<Pin targetPin>
    size_t
    psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<targetPin, LOW>();
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
        digitalWrite<targetPin, HIGH>();
        return count;
    }

    template<Pin targetPin>
    size_t
    psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
        digitalWrite<targetPin, LOW>();
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
        digitalWrite<targetPin, HIGH>();
        return count;
    }
}
size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite<Pin::PSRAM0>(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead<Pin::PSRAM0>(baseAddress, bytes, count);
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

uint16_t currentDataLinesValue = 0;


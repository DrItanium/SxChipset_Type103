/**
 * i960SxChipset_Type103
 * Copyright (c) 2022-2023, Joshua Scoggins
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include "xmem.h"
#ifdef I960_MEGA_MEMORY_CONTROLLER
#include "BankSelection.h"
#endif
#include "Types.h"
#include "Cache.h"
constexpr bool EnableDebugging = true;
constexpr auto PSRAMEnable = 2;
#ifndef SDCARD_SS_PIN
#define SDCARD_SS_PIN 10
#endif
constexpr auto SDPin = SDCARD_SS_PIN;
SdFat SD;
volatile bool systemBooted_ = false;

void onReceive(int) noexcept;
void onRequest() noexcept;


void
setupTWI() noexcept {
    Serial.print(F("Configuring TWI..."));
    Wire.begin(9);
#ifdef I960_MEGA_MEMORY_CONTROLLER
    Wire.setClock(400'000);
#endif
    Wire.onReceive(onReceive);
    Wire.onRequest(onRequest);
    Serial.println(F("DONE"));
}
void
setupSPI() noexcept {
    Serial.print(F("Configuring SPI..."));
    SPI.begin();
    Serial.println(F("DONE"));
}
void
setupSerial(bool displayBootScreen = true) noexcept {
    Serial.begin(115200);
    while(!Serial);
    if (displayBootScreen) {
        Serial.println(F("i960 Memory Controller (C) 2022 and beyond Joshua Scoggins"));
        Serial.println(F("This software is open source software"));
        Serial.println(F("Base Platform: Arduino Mega2560"));
    }
}
void
bringUpSDCard() noexcept {
    Serial.println(F("Bringing up SD Card support"));
    while (!SD.begin(SDPin)) {
        Serial.println(F("SD Card not found, waiting a second and trying again!"));
        delay(1000);
    }
    Serial.println(F("Brought up SD Card support"));
}
void
setupEBI() noexcept {
    xmem::begin(true);
}
void
configureGPIOs() noexcept {
    Serial.print(F("Configuring GPIOs..."));
#ifndef I960_GRAND_CENTRAL_M4_MEMORY_CONTROLLER
    pinMode(SDPin, OUTPUT);
    digitalWrite(SDPin, HIGH);
#endif
    pinMode(PSRAMEnable, OUTPUT);
    digitalWrite(PSRAMEnable, HIGH);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.println(F("DONE"));
}

template<int targetPin, bool performFullMemoryTest>
bool
setupPSRAM() noexcept {
    // according to the manuals we need at least 200 microseconds after bootup
    // to allow the psram to do it's thing
    delayMicroseconds(200);
    // 0x66 tells the PSRAM to initialize properly
    digitalWrite(targetPin, LOW);
    SPI.transfer(0x66);
    digitalWrite(targetPin, HIGH);
    // test the first 64k instead of the full 8 megabytes
    constexpr uint32_t endAddress = performFullMemoryTest ? 0x80'0000 : 0x10000;
    for (uint32_t i = 0; i < endAddress; i += 4) {
        SplitWord32 container {i}, result {0};
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x02); // write
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[0]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[3]);
        digitalWrite(targetPin, HIGH);
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        asm volatile ("nop");
        digitalWrite(targetPin, LOW);
        SPI.transfer(0x03); // read 
        SPI.transfer(container.bytes[2]);
        SPI.transfer(container.bytes[1]);
        SPI.transfer(container.bytes[0]);
        result.bytes[0] = SPI.transfer(0);
        result.bytes[1] = SPI.transfer(0);
        result.bytes[2] = SPI.transfer(0);
        result.bytes[3] = SPI.transfer(0);
        digitalWrite(targetPin, HIGH);
        if (container != result) {
            return false;
        }
    }
    return true;
}
template<bool performFullMemoryTest>
void
bringUpPSRAM() noexcept {
    uint32_t memoryAmount = 0;
    auto addPSRAMAmount = [&memoryAmount]() {
        memoryAmount += (8ul * 1024ul * 1024ul);
    };
    if (setupPSRAM<PSRAMEnable, performFullMemoryTest>()) {
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

namespace {
    template<int targetPin>
        size_t
        psramMemoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
            digitalWrite(targetPin, LOW);
#ifdef I960_MEGA_MEMORY_CONTROLLER
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
            digitalWrite(targetPin, HIGH);
            return count;
        }

    template<int targetPin>
        size_t
        psramMemoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
            digitalWrite(targetPin, LOW);
#ifdef I960_MEGA_MEMORY_CONTROLLER
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
            digitalWrite(targetPin, HIGH);
            return count;
        }
}

size_t
memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryWrite<PSRAMEnable>(baseAddress, bytes, count);
}
size_t
memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept {
    return psramMemoryRead<PSRAMEnable>(baseAddress, bytes, count);
}

template<typename T>
inline volatile T& memory(const size_t address) noexcept {
    return *reinterpret_cast<volatile T*>(address);
}
void 
installMemoryImage() noexcept {
    if (File memoryImage; !memoryImage.open("boot.sys", FILE_READ)) {
        Serial.println(F("Couldn't open boot.sys!"));
        while (true) {
            delay(1000);
        }
    } else {
        SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
        // write out to the data cache as we go along, when we do a miss then
        // we will be successful in writing out to main memory
        memoryImage.seekSet(0);
        Serial.println(F("installing memory image from sd"));
        // use bank0 for the transfer cache
        xmem::setMemoryBank(0);
        auto BufferSize = 16384;
        auto* buffer = new byte[BufferSize]();
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
        delete [] buffer;
        SPI.endTransaction();
    }
}
#ifdef I960_MEGA_MEMORY_CONTROLLER
CachePool<4, 8, 6, 6> thePool_;
#elif defined(I960_METRO_M4_MEMORY_CONTROLLER)
CachePool<4, 8, 0, 6> thePool_;
#elif defined(I960_GRAND_CENTRAL_M4_MEMORY_CONTROLLER)
CachePool<4, 9, 0, 6> thePool_;
#else
#error "Define Cache properly for target!"
#endif
void
setupCache() noexcept {
    // the pool will sit in the upper 64 elements
    thePool_.begin(16);
}


void 
setup() {
    setupSerial();
    setupEBI();
    setupCache();
    setupSPI();
    setupTWI();
    configureGPIOs();
    bringUpSDCard();
    bringUpPSRAM<false>();
    installMemoryImage();
    systemBooted_ = true;
    // setup cache in the heap now!
}



/*
 * Since requests are coming in over i2c, we have to be careful. I think we can
 * easily encode a system packet such that we are always swapping data in and
 * out. It is totally possible to request the new address and commit the old
 * data _after_ preparing the read operation. We can even queue the write
 * operation after the fact too. We have a read operation and a swap operation
 * only. I believe we will need to do a linked list implementation to support
 * this as well. 
 *
 * What needs to happen is that we have a small subset of actions to perform
 * which make sure that a given cache line is located in memory.
 *
 * The first thing to do is set the target cache line address. Then if we
 * perform a write at this point, it will be to the targetCacheLine address.
 * If it is a read then we find the cache line and perform the read right then
 * and there.
 *
 */
// at all times we have a front end cache line that we operate on
union Request {
    Request() : contents{0} { }
    byte contents[32];
    struct {
        byte direction;
        SplitWord32 baseAddress;
        byte data[16];
        SplitWord16 size;
    } packet;
};
volatile bool processingRequest = false;
volatile bool availableForRead = false;
volatile Request currentRequest;

void
onReceive(int howMany) noexcept {
    if (systemBooted_) {
        digitalWrite(LED_BUILTIN, HIGH);
        if (!processingRequest) {
            // only create a new request if we are idle
            processingRequest = true;
            availableForRead = false;
            for (int i = 0; i < howMany; ++i) {
                currentRequest.contents[i] = Wire.read();
            }
        }
        digitalWrite(LED_BUILTIN, LOW);
    }
}
constexpr byte BootingUp = 0xFF;
constexpr byte CurrentlyProcessingRequest = 0xFE;
constexpr byte NoCacheLineLoaded = 0xFD;
constexpr byte SuccessfulCacheLineRead = 0x55;
void
onRequest() noexcept {
    if (systemBooted_) {
        if (processingRequest) {
            Wire.write(CurrentlyProcessingRequest);
        } else {
            if (availableForRead) {
                Wire.write(SuccessfulCacheLineRead); // write a valid byte to differentiate things
                Wire.write(const_cast<byte*>(currentRequest.packet.data), 16);
            } else {
                // send a one byte message stating we are not ready for you yet
                // it can also mean we haven't requested anything yet
                Wire.write(NoCacheLineLoaded);
            }
        }
    } else {
        // we haven't booted the system yet!
        // return a single byte sequence
        Wire.write(BootingUp);
    }
}
// process the requests outside of the 
void 
loop() {
    if (processingRequest) {
        if constexpr (EnableDebugging) {
            Serial.println(F("Processing Request From: "));
            Serial.print(F("\t Address 0x"));
            Serial.println(currentRequest.packet.baseAddress.full, HEX);
        }
        // take the current request and process it
        auto& theLine = thePool_.find(const_cast<const SplitWord32&>(currentRequest.packet.baseAddress));
        decltype(thePool_)::CacheAddress targetAddress(const_cast<const SplitWord32&>(currentRequest.packet.baseAddress));
        if (currentRequest.packet.direction == 0) {
            if constexpr (EnableDebugging) {
                Serial.println(F("READ OPERATION!"));
            }
            // read operation
            for (int i = 0, j = targetAddress.getOffset(); i < 16; ++i, ++j) {
                currentRequest.packet.data[i] = theLine.read(j);
            }
        } else {
            if constexpr (EnableDebugging) {
                Serial.println(F("WRITE OPERATION!"));
            }
            // write operation
            for (int i = 0, j = targetAddress.getOffset(); i < 16; ++i, ++j) {
                theLine.write(j, currentRequest.packet.data[i]);
            }
        }
        if constexpr (EnableDebugging) {
            for (int i = 0; i < 16; ++i) {
                Serial.print(F("\tIndex: ")); 
                Serial.print(i);
                Serial.print(F(", Value: 0x")); 
                Serial.println(currentRequest.packet.data[i], HEX);
            }
            Serial.println(F("Finished Processing Request"));
        }
        availableForRead = true;
        processingRequest = false;
        // at the end we mark the cache line as available for reading
    }
}

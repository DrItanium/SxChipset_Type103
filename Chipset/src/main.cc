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
#include "Types.h"
#include "Pinout.h"
#include "Wire.h"
#include "Peripheral.h"
#include "Setup.h"
#include "SerialDevice.h"
#include "InfoDevice.h"
#include "TimerDevice.h"
#include "xmem.h"
//#include "Cache.h"
//SdFat SD;
// the logging shield I'm using has a DS1307 RTC
SerialDevice theSerial;
InfoDevice infoDevice;
TimerDevice timerInterface;

void 
putCPUInReset() noexcept {
    Platform::doReset(LOW);
}
void 
pullCPUOutOfReset() noexcept {
    Platform::doReset(HIGH);
}
[[gnu::always_inline]]
inline void 
waitForDataState() noexcept {
    singleCycleDelay();
    while (digitalRead<Pin::DEN>() == HIGH);
}
template<bool isReadOperation, typename T>
inline void
talkToi960(const SplitWord32& addr, T& handler) noexcept {
    handler.startTransaction(addr);
    do {
        auto c0 = readInputChannelAs<Channel0Value, true>();
        if constexpr (EnableDebugMode) {
            Serial.print(F("\tChannel0: 0b"));
            Serial.println(static_cast<int>(c0.getWholeValue()), BIN);
        }
        if constexpr (isReadOperation) {
            // okay it is a read operation, so... pull a cache line out
            auto value = handler.read(c0);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tGot Value: 0x"));
                Serial.println(value, HEX);
            }
            Platform::setDataLines(value);
        } else {
            auto value = Platform::getDataLines();
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            // so we are writing to the cache
            handler.write(c0, value);
        }
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        signalReady();
        if (isBurstLast) {
            break;
        }
        handler.next();
        singleCycleDelay(); // put this in to make sure we never over run anything
    } while(true);
    handler.endTransaction();
}

struct TreatAsCacheAccess final { };
struct TreatAsOnChipAccess final { };

#if 0
template<bool isReadOperation>
inline void
talkToi960(const SplitWord32& addr, TreatAsCacheAccess) noexcept {
    auto &line = getCache().find(addr);
    // the compiler seems to barf on for loops at -Ofast
    // so instead, we want to unpack it to make sure
    for (auto offset = (MemoryCache::CacheAddress{addr}.getOffset() >> 1); ; ++offset) {
        auto c0 = readInputChannelAs<Channel0Value, true>();
        if constexpr (EnableDebugMode) {
            Serial.print(F("\tChannel0: 0b"));
            Serial.println(static_cast<int>(c0.getWholeValue()), BIN);
        }
        if constexpr (isReadOperation) {
            // okay it is a read operation, so... pull a cache line out
            auto value = line.getWord(offset);
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tRead Value: 0x"));
                Serial.println(value, HEX);
            }
            Platform::setDataLines(value);
        } else {
            auto value = Platform::getDataLines();
            if constexpr (EnableDebugMode) {
                Serial.print(F("\t\tWrite Value: 0x"));
                Serial.println(value, HEX);
            }
            line.setWord(offset, value, c0.getByteEnable());
        }
        auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        signalReady();
        if (isBurstLast) {
            break;
        }
        // make sure that we have enough time as the chip is pipelined
        singleCycleDelay();
    }
    if constexpr (!isReadOperation) {
        line.markDirty();
    }
}
#endif

template<bool isReadOperation>
inline void
talkToi960(const SplitWord32& addr, TreatAsOnChipAccess) noexcept {
    if (auto theIndex = addr.onBoardMemoryAddress.bank + 8; xmem::validBank(theIndex)) {
        xmem::setMemoryBank(theIndex);
        volatile SplitWord16* ptr = reinterpret_cast<volatile SplitWord16*>((RAMEND + 1) + addr.onBoardMemoryAddress.offset);
        do {
            auto c0 = readInputChannelAs<Channel0Value, true>();
            if constexpr (EnableDebugMode) {
                Serial.print(F("\tChannel0: 0b"));
                Serial.println(static_cast<int>(c0.getWholeValue()), BIN);
            }
            if constexpr (isReadOperation) {
                // keep setting the data lines and inform the i960
                Platform::setDataLines(ptr->full);
            } else {
                switch (c0.getByteEnable()) {
                    case EnableStyle::Full16:
                        ptr->full = Platform::getDataLines();
                        break;
                    case EnableStyle::Lower8:
                        // directly read from the ports to speed things up
                        ptr->bytes[0] = getInputRegister<Port::DataLower>();
                        break;
                    case EnableStyle::Upper8:
                        ptr->bytes[1] = getInputRegister<Port::DataUpper>();
                        break;
                    default:
                        break;
                }
            }
            auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
            signalReady();
            if (isBurstLast) {
                break;
            }
            ++ptr;
            singleCycleDelay(); // put this in to make sure we never over run anything
        } while (true);
    } else {
        // if they are not valid addresses then use the null handler
        talkToi960<isReadOperation>(addr, getNullHandler());
    }
}
template<bool isReadOperation>
void
//inline TransactionInterface&
getPeripheralDevice(const SplitWord32& addr) noexcept {
    switch (addr.getIODevice<TargetPeripheral>()) {
        case TargetPeripheral::Info:
            talkToi960<isReadOperation>(addr, infoDevice);
            break;
        case TargetPeripheral::Serial:
            talkToi960<isReadOperation>(addr, theSerial);
            break;
        case TargetPeripheral::RTC:
            talkToi960<isReadOperation>(addr, timerInterface);
            break;
        default:
            talkToi960<isReadOperation>(addr, getNullHandler());
            break;
    }
}

template<bool isReadOperation>
void
//inline TransactionInterface&
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
            getPeripheralDevice<isReadOperation>(addr);
            break;
        default:
            talkToi960<isReadOperation>(addr, getNullHandler());
            break;
    }
}

inline void
handleTransaction() noexcept {
    Platform::startAddressTransaction();
    Platform::collectAddress();
    Platform::endAddressTransaction();
    // don't do virtual address translation here
    // for our purposes, we want to make sure that this code is a simple as possible
    auto addr = Platform::getAddress();
    if constexpr (EnableDebugMode) {
        Serial.print(F("Address: 0x"));
        Serial.print(addr.getWholeValue(), HEX);
        Serial.print(F(" (0b"));
        Serial.print(addr.getWholeValue(), BIN);
        Serial.println(F(")"));
        Serial.print(F("Operation: "));
        if (Platform::isReadOperation()) {
            Serial.println(F("Read!"));
        } else {
            Serial.println(F("Write!"));
        }
    }
    if (addr.isIOInstruction()) {
        if (Platform::isReadOperation()) {
            handleIOOperation<true>(addr);
        } else {
            handleIOOperation<false>(addr);
        }
    } else {
        if (Platform::isReadOperation()) {
            talkToi960<true>(addr, TreatAsOnChipAccess{});
        } else {
            talkToi960<false>(addr, TreatAsOnChipAccess{});
        }
    }
    // allow for extra recovery time, introduce a single 10mhz cycle delay
    // shift back to input channel 0
    singleCycleDelay();
}
template<bool TrackBootProcess = false>
void
bootCPU() noexcept {
    pullCPUOutOfReset();
    // I used to keep track of the boot process but I realized that we don't actually need to do this
    // It just increases overhead and can slow down the boot process. A checksum fail will still halt the CPU
    // as expected. We just sit there waiting for something that will never continue. This is fine.

    // The boot process is left here just incase we need to reactivate it
    if constexpr (TrackBootProcess) {
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
        waitForDataState();
        handleTransaction();
        waitForDataState();
        handleTransaction();
        if (digitalRead<Pin::FAIL>() == HIGH) {
            Serial.println(F("CHECKSUM FAILURE!"));
        } else {
            Serial.println(F("BOOT SUCCESSFUL!"));
        }
    }
}
void
setup() {
    theSerial.begin();
    infoDevice.begin();
    timerInterface.begin();
    Wire.begin();
    SPI.begin();
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    // setup the IO Expanders
    Platform::begin();
    delay(1000);
    // jump into the monitor rom to help me setup the i960 memory before jumping to it
    bootCPU();
}


void 
loop() {
    waitForDataState();
    handleTransaction();
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


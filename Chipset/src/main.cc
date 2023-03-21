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
#include "Peripheral.h"
#include "Setup.h"
#include "SerialDevice.h"
#include "InfoDevice.h"
#include "TimerDevice.h"
#include <SD.h>
#include "BankSelection.h"
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
void
waitForDataState() noexcept {
    Platform::waitForDataState();
}
template<bool isReadOperation>
struct RWOperation final {};
using ReadOperation = RWOperation<true>;
using WriteOperation = RWOperation<false>;
struct LoadFromEBI final { };
struct LoadFromPortK final { };
struct LoadFromIBUS final { };

using SelectedLogic = LoadFromIBUS;

template<typename T>
[[gnu::always_inline]]
inline void
manipulateHandler(T& handler, WriteOperation) noexcept {
    auto value = Platform::getDataLines();
    if constexpr (EnableDebugMode) {
        Serial.print(F("\t\tWrite Value: 0x"));
        Serial.println(value, HEX);
    }
    // so we are writing to the cache
    handler.write(value);
}

template<typename T>
[[gnu::always_inline]]
inline void
manipulateHandler(T& handler, ReadOperation) noexcept {
    // okay it is a read operation, so... pull a cache line out
    auto value = handler.read();
    if constexpr (EnableDebugMode) {
        Serial.print(F("\t\tGot Value: 0x"));
        Serial.println(value, HEX);
    }
    Platform::setDataLines(value);
}
template<bool isReadOperation, typename T>
inline void
talkToi960(const SplitWord32& addr, T& handler) noexcept {
    handler.startTransaction(addr);
    // previously, we were doing up to eight operations but things have changed
    // since we are now operating on a 32-bit bus
    while (true) {
        manipulateHandler(handler, RWOperation<isReadOperation>{});
        auto isDone = Platform::isBurstLast();
        signalReady();
        if (isDone) {
            return;
        }
        handler.next();
    }
}

struct TreatAsOnChipAccess final { };

[[gnu::always_inline]]
inline void
manipulateDataLines(const SplitWord16* const ptr, ReadOperation) noexcept {
#if 0
    if constexpr (EnableDebugMode) {
        Serial.print(F("\tRead Out 0x"));
        Serial.println(ptr->getWholeValue(), HEX);
    }
    // keep setting the data lines and inform the i960
    setDataLines(ptr->getWholeValue());
#endif
}

[[gnu::always_inline]]
inline void
manipulateDataLines(SplitWord16* ptr, WriteOperation) noexcept {
#if 0
    if constexpr (EnableDebugMode) {
        Serial.print(F("\tWrite in 0x"));
        Serial.println(Platform::getDataLines(), HEX);
    }
    if (digitalRead<Pin::BE0>() == LOW) {
        ptr->bytes[0] = getInputRegister<Port::DataLower>();
    }
    if (digitalRead<Pin::BE1>() == LOW) {
        ptr->bytes[1] = getInputRegister<Port::DataUpper>();
    }
#endif
}
template<bool isReadOperation, ByteEnableKind kind>
struct RequestProcessor {
    static void execute(const SplitWord32& addr, TreatAsOnChipAccess) noexcept {
        auto& ptr = Platform::adjustMemoryView(addr);
        if constexpr (constexpr uint8_t theBits = static_cast<uint8_t>(kind); isReadOperation) {
            if constexpr (theBits & 0b0001) {
                Platform::setDataByte(0, ptr.bytes[0]);
            }
            if constexpr (theBits & 0b0010) {
                Platform::setDataByte(1, ptr.bytes[1]);
            }
            if constexpr (theBits & 0b0100) {
                Platform::setDataByte(2, ptr.bytes[2]);
            }
            if constexpr (theBits & 0b1000) {
                Platform::setDataByte(3, ptr.bytes[3]);
            }
        } else {
            if constexpr (theBits & 0b0001) {
                ptr.bytes[0] = Platform::getDataByte(0);
            }
            if constexpr (theBits & 0b0010) {
                ptr.bytes[1] = Platform::getDataByte(1);
            }
            if constexpr (theBits & 0b0100) {
                ptr.bytes[2] = Platform::getDataByte(2);
            }
            if constexpr (theBits & 0b1000) {
                ptr.bytes[3] = Platform::getDataByte(3);
            }
        }
    }
};
#define BeginRequestHandler(kind) \
    template<bool isReadOperation> \
    struct RequestProcessor<isReadOperation, ByteEnableKind:: kind > { \
        static void execute(const SplitWord32& addr, TreatAsOnChipAccess) noexcept { 

#define EndRequestHandler } };

BeginRequestHandler(Full32) 
    // set the appropriate bank index
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setDataLines(ptr.full);
    } else {
        ptr.full = Platform::getDataLines();
    }
EndRequestHandler

BeginRequestHandler(Lower16)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setLowerDataBits(ptr.halves[0]);
    } else {
        ptr.halves[0] = Platform::getLowerDataBits();
    }
EndRequestHandler

BeginRequestHandler(Upper16)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setUpperDataBits(ptr.halves[1]);
    } else {
        ptr.halves[1] = Platform::getUpperDataBits();
    }
EndRequestHandler

BeginRequestHandler(Lowest8)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setDataByte(0, ptr.bytes[0]);
    } else {
        ptr.bytes[0] = Platform::getDataByte(0);
    }
EndRequestHandler

BeginRequestHandler(Lower8)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setDataByte(1, ptr.bytes[1]);
    } else {
        ptr.bytes[1] = Platform::getDataByte(1);
    }
EndRequestHandler

BeginRequestHandler(Higher8)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setDataByte(2, ptr.bytes[2]);
    } else {
        ptr.bytes[2] = Platform::getDataByte(2);
    }
EndRequestHandler

BeginRequestHandler(Highest8)
    auto& ptr = Platform::adjustMemoryView(addr);
    if constexpr (isReadOperation) {
        Platform::setDataByte(3, ptr.bytes[3]);
    } else {
        ptr.bytes[3] = Platform::getDataByte(3);
    }
EndRequestHandler

#undef BeginRequestHandler
#undef EndRequestHandler



template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(const SplitWord32& addr, TreatAsOnChipAccess) noexcept {
    /// @todo figure out which bank we are a part of on the IBUS
    do {
        switch (static_cast<ByteEnableKind>(Platform::getByteEnable())) {
#define X(frag) case ByteEnableKind:: frag : RequestProcessor< isReadOperation , ByteEnableKind:: frag > :: execute (addr, TreatAsOnChipAccess {}); break
            X(Full32);
            X(Lower16);
            X(Upper16);
            X(Lowest8);
            X(Lower8);
            X(Higher8);
            X(Highest8);
            X(Mid16);
            X(Lower24);
            X(Upper24);
            X(Nothing);
            X(Highest8_Lower16 );
            X(Highest8_Lower8 );
            X(Highest8_Lowest8 );
            X(Upper16_Lowest8 );
            X(Higher8_Lowest8 );
#undef X
            default:
                /// @todo implement simple walkthrough
                break;
        }
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
    } while (true);
#if 0
    BankSwitcher::setBank(addr.compute328BusBank());
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target Bank: 0x"));
        Serial.println(static_cast<uint32_t>(addr.compute328BusBank()), HEX);
    }
    SplitWord16* ptr = reinterpret_cast<SplitWord16*>(addr.compute328BusAddress());
    if constexpr (EnableDebugMode) {
        Serial.print(F("Target Address: 0x"));
        Serial.println(addr.compute328BusAddress(), HEX);
    }
    using TargetOp = RWOperation<isReadOperation>;
    for (byte i = 0; i < 8; ++i) {
        manipulateDataLines(ptr, TargetOp{});
        //auto isBurstLast = digitalRead<Pin::BLAST_>() == LOW;
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
        ++ptr;
    }
#endif
}
template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(uint32_t addr, TreatAsOnChipAccess) noexcept {
    talkToi960<isReadOperation>(SplitWord32{addr}, TreatAsOnChipAccess{});
}
template<bool isReadOperation>
void
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
            talkToi960<isReadOperation>(addr, TreatAsOnChipAccess{});
            break;
    }
}
template<bool isReadOperation>
[[gnu::always_inline]]
inline void
getPeripheralDevice(uint32_t addr) noexcept {
    getPeripheralDevice<isReadOperation>(SplitWord32{addr});
}

#if 0
[[gnu::always_inline]]
inline void
triggerClock() noexcept {
    // just write to the CLKSignal pin to force a toggle
    Platform::signalReady();
    //pulse<Pin::CLKSignal>();
    singleCycleDelay();
}
#endif
inline void
handleTransaction(LoadFromIBUS) noexcept {
    // first we need to extract the address from the CH351s
    auto address = Platform::readAddress();
    if (Platform::isReadOperation()) {
        Platform::configureDataLinesForRead();
        if (Platform::isIOOperation()) {
            getPeripheralDevice<true>(address);
        } else {
            talkToi960<true>(address, TreatAsOnChipAccess{});
        }
    } else {
        Platform::configureDataLinesForWrite();
        if (Platform::isIOOperation()) {
            getPeripheralDevice<false>(address);
        } else {
            talkToi960<false>(address, TreatAsOnChipAccess{});
        }
    }
    // need this delay to synchronize everything :)
    singleCycleDelay();
}
inline void
handleTransaction(LoadFromPortK) noexcept {
#if 0
    SplitWord32 addr{0};
    // clear the address counter to be on the safe side
    if constexpr (EnableDebugMode) {
        digitalWrite<Pin::AddressCaptureSignal1, LOW>();
    }
    triggerClock();
    toggle<Pin::Enable>();
    singleCycleDelay(); // introduce this extra cycle of delay to make sure
    // that inputs are updated correctly since they are
    // tristated
    if (auto m2 = readInputChannelAs<uint8_t>(); (m2 & 0b1) == 0) {
        addr.bytes[0] = m2;
        // unpack the signal triggering to interleave the act of updating direction with waiting for the clock signalling
        pulse<Pin::CLKSignal>();
        getDirectionRegister<Port::DataLower>() = 0xFF;
        getDirectionRegister<Port::DataUpper>() = 0xFF;
        auto b1 = readInputChannelAs<uint8_t>();
        pulse<Pin::CLKSignal>();
        addr.bytes[1] = b1;
        auto b2 = readInputChannelAs<uint8_t>();
        pulse<Pin::CLKSignal>();
        addr.bytes[2] = b2;
        auto b3 = readInputChannelAs<uint8_t>();
        toggle<Pin::Enable>();
        pulse<Pin::CLKSignal>();
        addr.bytes[3] = b3;
        // When we are in io space, we are treating the address as an opcode which
        // we can decompose while getting the pieces from the io expanders. Thus we
        // can overlay the act of decoding while getting the next part
        //
        // The W/~R pin is used to figure out if this is a read or write operation
        //
        // This system does not care about the size but it does care about where
        // one starts when performing a write operation
        if (b3 == 0xF0) {
            getPeripheralDevice<true>(addr);
        } else {
            talkToi960<true>(addr, TreatAsOnChipAccess{});
        }
    } else {
        /// @todo do we need to the masking now that the caches have been removed?
        addr.bytes[0] = m2 & 0b1111'1110;
        pulse<Pin::CLKSignal>();
        getDirectionRegister<Port::DataLower>() = 0;
        getDirectionRegister<Port::DataUpper>() = 0;
        auto b1 = readInputChannelAs<uint8_t>();
        pulse<Pin::CLKSignal>();
        addr.bytes[1] = b1;
        auto b2 = readInputChannelAs<uint8_t>();
        pulse<Pin::CLKSignal>();
        addr.bytes[2] = b2;
        auto b3 = readInputChannelAs<uint8_t>();
        toggle<Pin::Enable>();
        pulse<Pin::CLKSignal>();
        addr.bytes[3] = b3;

        // When we are in io space, we are treating the address as an opcode which
        // we can decompose while getting the pieces from the io expanders. Thus we
        // can overlay the act of decoding while getting the next part
        //
        // The W/~R pin is used to figure out if this is a read or write operation
        //
        // This system does not care about the size but it does care about where
        // one starts when performing a write operation
        if (b3 == 0xF0) {
            getPeripheralDevice<false>(addr);
        } else {
            talkToi960<false>(addr, TreatAsOnChipAccess{});
        }
    }
    // allow for extra recovery time, introduce a single 10mhz cycle delay
    // shift back to input channel 0
    singleCycleDelay();
#endif
}

void
handleTransaction(LoadFromEBI) noexcept {
#if 0
    // clear the address counter to be on the safe side
    SplitWord32 addr{memory<uint32_t>(0x7F00)};
    //singleCycleDelay(); // introduce this extra cycle of delay to make sure
    // that inputs are updated correctly since they are
    // tristated
    if ((addr.bytes[0] & 0b1) == 0) {
        memory<uint16_t>(0x7F10) = 0xFFFF;
        // When we are in io space, we are treating the address as an opcode which
        // we can decompose while getting the pieces from the io expanders. Thus we
        // can overlay the act of decoding while getting the next part
        //
        // The W/~R pin is used to figure out if this is a read or write operation
        //
        // This system does not care about the size but it does care about where
        // one starts when performing a write operation
        if (addr.isIOInstruction()) {
            getPeripheralDevice<true>(addr);
        } else {
            talkToi960<true>(addr, TreatAsOnChipAccess{});
        }
    } else {
        /// @todo do we need to the masking now that the caches have been removed?
        addr.bytes[0] &= 0b1111'1110;
        memory<uint16_t>(0x7F10) = 0;

        // When we are in io space, we are treating the address as an opcode which
        // we can decompose while getting the pieces from the io expanders. Thus we
        // can overlay the act of decoding while getting the next part
        //
        // The W/~R pin is used to figure out if this is a read or write operation
        //
        // This system does not care about the size but it does care about where
        // one starts when performing a write operation
        if (addr.isIOInstruction()) {
            getPeripheralDevice<false>(addr);
        } else {
            talkToi960<false>(addr, TreatAsOnChipAccess{});
        }
    }
    // allow for extra recovery time, introduce a single 10mhz cycle delay
    // shift back to input channel 0
    singleCycleDelay();
#endif
}

void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = 512ul * 1024ul;
    while (!SD.begin(static_cast<int>(Pin::SD_EN))) {
        Serial.println(F("NO SD CARD!"));
        delay(1000);
    }
    Serial.println(F("SD CARD FOUND!"));
    // look for firmware.bin and open it readonly
    if (auto theFirmware = SD.open("firmware.bin", FILE_READ); !theFirmware) {
        Serial.println(F("Could not open firmware.bin for reading!"));
        while (true) {
            delay(1000);
        }
    } else if (theFirmware.size() > MaximumFileSize) {
        Serial.println(F("The firmware image is too large to fit in 512k of sram!"));
        while (true) {
            delay(1000);
        }
    } else {
        constexpr auto BufferSize = 16384;

        auto previousBank = BankSwitcher::getBank();
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            BankSwitcher::setBank(view.compute328BusBank());
            uint8_t* theBuffer = reinterpret_cast<uint8_t*>(view.compute328BusAddress());
            theFirmware.read(theBuffer, BufferSize);
            Serial.print(F("."));
        }
        Serial.println(F("DONE!"));
        theFirmware.close();
        BankSwitcher::setBank(previousBank);
    }
}
void
setup() {
    theSerial.begin();
    infoDevice.begin();
    timerInterface.begin();
    SPI.begin();
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    // setup the IO Expanders
    Platform::begin();
    delay(1000);
    // find firmware.bin and install it into the 512k block of memory
    installMemoryImage();
    pullCPUOutOfReset();
}


void 
loop() {
    while (true) {
        waitForDataState();
        handleTransaction(SelectedLogic{});
    }
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


#ifdef INT7_vect
ISR(INT7_vect) {
    // since we are using PE7 as CLKO, this vector can be leveraged for some
    // sort of internal operation if desired
}
#endif

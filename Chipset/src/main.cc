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

struct TreatAsOnChipAccess final { };
struct TreatAsOffChipAccess final { };
struct TreatAsInstruction final { };


template<bool isReadOperation, ByteEnableKind kind>
struct RequestProcessor {
private:
    static constexpr uint8_t ByteEnableAsBits = static_cast<uint8_t>(kind);
    static void performEBIExecution(volatile SplitWord32& ptr) noexcept {
        // stream the data out onto the bus
        // at some point this code may go away and instead we only respond when
        // being talked to.
        if constexpr (isReadOperation) {
            fulfillIOExpanderReads(const_cast<const SplitWord32&>(ptr));
        } else {
            fulfillIOExpanderWrites(ptr);
        }
    }
    static void fulfillIOExpanderReads(const SplitWord32& ptr) noexcept {
        if constexpr (isReadOperation) {
            if constexpr (ByteEnableAsBits == 0) {
                Platform::setDataLines(ptr.full);
            } else if constexpr (ByteEnableAsBits == 0b0011) {
                Platform::setUpperDataBits(ptr.halves[1]);
            } else if constexpr (ByteEnableAsBits == 0b1100) {
                Platform::setLowerDataBits(ptr.halves[0]);
            } else {
                if constexpr ((ByteEnableAsBits & 0b0001) == 0) {
                    Platform::setDataByte(0, ptr.bytes[0]);
                }
                if constexpr ((ByteEnableAsBits & 0b0010) == 0) {
                    Platform::setDataByte(1, ptr.bytes[1]);
                }
                if constexpr ((ByteEnableAsBits & 0b0100) == 0) {
                    Platform::setDataByte(2, ptr.bytes[2]);
                }
                if constexpr ((ByteEnableAsBits & 0b1000) == 0) {
                    Platform::setDataByte(3, ptr.bytes[3]);
                }
            }
        } 
        // do nothing on writes
    }
    static void fulfillIOExpanderWrites(volatile SplitWord32& ptr) noexcept {
        if constexpr (!isReadOperation) {
            if constexpr (ByteEnableAsBits == 0) {
                ptr.full = Platform::getDataLines();
            } else if constexpr (ByteEnableAsBits == 0b0011) {
                ptr.halves[1] = Platform::getUpperDataBits();
            } else if constexpr (ByteEnableAsBits == 0b1100) {
                ptr.halves[0] = Platform::getLowerDataBits();
            } else {
                if constexpr ((ByteEnableAsBits & 0b0001) == 0) {
                    ptr.bytes[0] = Platform::getDataByte(0);
                }
                if constexpr ((ByteEnableAsBits & 0b0010) == 0) {
                    ptr.bytes[1] = Platform::getDataByte(1);
                }
                if constexpr ((ByteEnableAsBits & 0b0100) == 0) {
                    ptr.bytes[2] = Platform::getDataByte(2);
                }
                if constexpr ((ByteEnableAsBits & 0b1000) == 0) {
                    ptr.bytes[3] = Platform::getDataByte(3);
                }
            }
        }
    }
public:
    static void execute(const SplitWord32& addr, TreatAsOnChipAccess) noexcept {
        performEBIExecution( Platform::getMemoryView(addr, AccessFromIBUS { }));
    }
    static void execute(const SplitWord32& addr, TreatAsOffChipAccess) noexcept {
        performEBIExecution( Platform::getMemoryView(addr, AccessFromXBUS { }));
    }
    static void execute(Instruction& container, TreatAsInstruction) noexcept {
        if constexpr (isReadOperation) {
            // assume that we have performed the read operation ahead of time
            // and that we are operating on the packet we got back
            fulfillIOExpanderReads(container.args_[(Platform::getAddressOffset() & 0b1100) >> 2]);
        } else {
            fulfillIOExpanderWrites(container.args_[(Platform::getAddressOffset() & 0b1100) >> 2]);
        }
    }
};
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

template<bool isReadOperation>
inline void
talkToi960(const SplitWord32& addr, TreatAsInstruction) noexcept {
    Instruction operation;
    operation.opcode_ = addr;
    do {
        switch (static_cast<ByteEnableKind>(Platform::getByteEnable())) {
#define X(frag) case ByteEnableKind:: frag : RequestProcessor< isReadOperation , ByteEnableKind:: frag > :: execute (operation, TreatAsInstruction{}); break
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
            X(Highest8_Lower16 );
            X(Highest8_Lower8 );
            X(Highest8_Lowest8 );
            X(Upper16_Lowest8 );
            X(Higher8_Lowest8 );
#undef X
            default:
                RequestProcessor<isReadOperation, ByteEnableKind::Nothing>::execute(operation, TreatAsInstruction{});
                break;
        }
    } while (true);
    if constexpr (!isReadOperation) {
        // at this point, we process the instruction instead of trying to do it
        // inline
    }
}



template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(const SplitWord32& addr, TreatAsOnChipAccess) noexcept {
    // only need to set this once, it is literally impossible for this to span
    // banks
    Platform::setBank(addr, AccessFromIBUS{});
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
            X(Highest8_Lower16 );
            X(Highest8_Lower8 );
            X(Highest8_Lowest8 );
            X(Upper16_Lowest8 );
            X(Higher8_Lowest8 );
#undef X
            default:
                RequestProcessor<isReadOperation, ByteEnableKind::Nothing>::execute(addr, TreatAsOnChipAccess{});
                break;
        }
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
    } while (true);
}
template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(const SplitWord32& addr, TreatAsOffChipAccess) noexcept {
    // only need to set this once, it is literally impossible for this to span
    // banks
    Platform::setBank(addr, AccessFromXBUS{});
    do {
        switch (static_cast<ByteEnableKind>(Platform::getByteEnable())) {
#define X(frag) case ByteEnableKind:: frag : RequestProcessor< isReadOperation , ByteEnableKind:: frag > :: execute (addr, TreatAsOffChipAccess {}); break
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
            X(Highest8_Lower16 );
            X(Highest8_Lower8 );
            X(Highest8_Lowest8 );
            X(Upper16_Lowest8 );
            X(Higher8_Lowest8 );
#undef X
            default:
                RequestProcessor<isReadOperation, ByteEnableKind::Nothing>::execute(addr, TreatAsOffChipAccess{});
                break;
        }
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
    } while (true);
}
template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(uint32_t addr, TreatAsOnChipAccess) noexcept {
    talkToi960<isReadOperation>(SplitWord32{addr}, TreatAsOnChipAccess{});
}
template<bool isReadOperation>
[[gnu::always_inline]]
inline void
talkToi960(uint32_t addr, TreatAsOffChipAccess) noexcept {
    talkToi960<isReadOperation>(SplitWord32{addr}, TreatAsOffChipAccess{});
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

        auto previousBank = Platform::getBank(AccessFromIBUS{});
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            Platform::setBank(view, AccessFromIBUS{});
            uint8_t* theBuffer = const_cast<uint8_t*>(&memory<uint8_t>(view, AccessFromIBUS{}));
            theFirmware.read(theBuffer, BufferSize);
            Serial.print(F("."));
        }
        Serial.println(F("DONE!"));
        theFirmware.close();
        Platform::setBank(previousBank, AccessFromIBUS{});
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

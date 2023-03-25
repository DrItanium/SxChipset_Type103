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
struct RWOperation final {
    static constexpr bool IsReadOperation = isReadOperation;
};
using ReadOperation = RWOperation<true>;
using WriteOperation = RWOperation<false>;
struct LoadFromIBUS final { };
struct LoadFromXBUS final { };
using SelectedLogic = LoadFromIBUS;

struct TreatAsOnChipAccess final {
    using AccessMethod = AccessFromIBUS;
};
struct TreatAsOffChipAccess final {
    using AccessMethod = AccessFromXBUS;
};
struct TreatAsInstruction final { 
    using AccessMethod = AccessFromInstruction;
};


template<bool inDebugMode, bool isReadOperation, ByteEnableKind kind>
struct RequestProcessor {
private:
    static constexpr uint8_t ByteEnableAsBits = static_cast<uint8_t>(kind);
    static void performEBIExecution(volatile SplitWord32& ptr) noexcept {
        // stream the data out onto the bus
        // at some point this code may go away and instead we only respond when
        // being talked to.
        //
        if constexpr (isReadOperation) {
            // make a copy to ensure that we read from memory when it is important
            fulfillIOExpanderReads(ptr);
        } else {
            // directly use the contents
            fulfillIOExpanderWrites(ptr);
        }
    }
    static void fulfillIOExpanderReads(volatile SplitWord32& ptr) noexcept {
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
            if constexpr (inDebugMode) {
                Serial.print(F("Read Operation: IOEXP is now 0x"));
                auto theValue = getProcessorInterface().dataLines_.view32.data;
                Serial.println(theValue, HEX);
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
            if constexpr (inDebugMode) {
                Serial.print(F("Write Operation: IOEXP is now 0x"));
                auto theValue = ptr.full;
                Serial.println(theValue, HEX);
            }
        }
    }
public:
    template<typename T>
    static void execute(const SplitWord32& addr, T) noexcept {
        // get the closest word from where we currently are
        volatile SplitWord32& result = Platform::getMemoryView(addr, T{});
        if constexpr (inDebugMode) {
            Serial.print(F("Target Base Address: 0x"));
            Serial.println(reinterpret_cast<size_t>(&result), HEX);
            Serial.print(F("Target Base Address Contents: 0x"));
            Serial.println(result.full, HEX);
        }
        performEBIExecution( result);
    }
    static void execute(Instruction& container, AccessFromInstruction) noexcept {

        if constexpr (uint8_t targetWordIndex = ((Platform::getAddressOffset() >> 2) & 0b11); isReadOperation) {
            // assume that we have performed the read operation ahead of time
            // and that we are operating on the packet we got back
            fulfillIOExpanderReads(container.args_[targetWordIndex]);
        } else {
            fulfillIOExpanderWrites(container.args_[targetWordIndex]);
        }
    }
};
template<bool inDebugMode>
void
performIOReadGroup0(Instruction& instruction) noexcept {
    // unlike standard i960 operations, we only encode the data we actually care
    // about out of the packet when performing a read operation so at this
    // point it doesn't matter what kind of data the i960 is requesting.
    // This maintains consistency and makes the implementation much simpler
    switch (instruction.getDevice()) {
        case TargetPeripheral::Info:
            infoDevice.performRead(instruction);
            break;
        case TargetPeripheral::Serial:
            theSerial.performRead(instruction);
            break;
        case TargetPeripheral::Timer:
            timerInterface.performRead(instruction);
            break;
        default:
            // unknown device so do not do anything
            break;
    }
}
template<bool inDebugMode>
void
performIOWriteGroup0(const Instruction& instruction) noexcept {
    // unlike standard i960 operations, we only decode the data we actually care
    // about out of the packet when performing a write operation so at this
    // point it doesn't matter what kind of data we were actually given
    switch (instruction.getDevice()) {
        case TargetPeripheral::Info:
            infoDevice.performWrite(instruction);
            break;
        case TargetPeripheral::Serial:
            theSerial.performWrite(instruction);
            break;
        case TargetPeripheral::Timer:
            timerInterface.performWrite(instruction);
            break;
        default:
            // unknown device so do not do anything
            break;
    }
}
template<bool inDebugMode, bool isReadOperation>
void
talkToi960(uint32_t theAddr, TreatAsInstruction) noexcept {
    SplitWord32 addr{theAddr};
    // If the lowest four bits are not zero then that is a problem on the side
    // of the i960. For example, if you started in the middle of the 16-byte
    // block then the data will still be valid just not what you wanted. This
    // ignores a huge class of problems. Instead, we just process
    Instruction operation;
    operation.opcode_ = addr;
    if constexpr (isReadOperation) {
        // We perform the read operation _ahead_ of sending it back to the i960
        // The result of the read operation is stored in the args of the
        // instruction. 
        // that is what's performed here
        switch (operation.getGroup()) {
            case 0x0: // only group 0 is active right now
                performIOReadGroup0<inDebugMode>(operation);
                break;
            default:
                // just ignore the data and return what we have inside of
                // the operation object itself. 
                //
                // This greatly simplifies everything!
                return;
        }
    }
    // after we get the instruction configured (regardless of read or write) we
    // interact with the i960 to either send data off or not
    do {
        switch (static_cast<ByteEnableKind>(Platform::getByteEnable())) {
#define X(frag) case ByteEnableKind:: frag : RequestProcessor< inDebugMode, isReadOperation , ByteEnableKind:: frag > :: execute (operation, typename TreatAsInstruction::AccessMethod{}); break
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
            RequestProcessor<inDebugMode, isReadOperation, ByteEnableKind::Nothing>::execute(operation, typename TreatAsInstruction::AccessMethod{});
            break;
        }
    } while (true);
    if constexpr (!isReadOperation) {
        switch (operation.getGroup()) {
            case 0x0:
                performIOWriteGroup0<inDebugMode>(operation);
                break;
            default:
                // if we got here then do nothing, usually that means the
                // group hasn't been fully implemented yet
                break;
        }
    }
}



template<bool inDebugMode, bool isReadOperation, typename T>
void
talkToi960(uint32_t theAddr, T) noexcept {
    SplitWord32 addr{theAddr};
    // only need to set this once, it is literally impossible for this to span
    // banks
    Platform::setBank(addr, typename T::AccessMethod{});
    do {
        switch (static_cast<ByteEnableKind>(Platform::getByteEnable())) {
#define X(frag) case ByteEnableKind:: frag : RequestProcessor< inDebugMode, isReadOperation , ByteEnableKind:: frag > :: execute (addr, typename T::AccessMethod {}); break;
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
                RequestProcessor<inDebugMode, isReadOperation, ByteEnableKind::Nothing>::execute(addr, typename T::AccessMethod{});
                break;
        }
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
        // update the LSB since we are moving on
        addr.bytes[0] = Platform::getAddressLSB();
    } while (true);
}


template<bool inDebugMode>
void
handleTransaction(LoadFromIBUS) noexcept {
    // first we need to extract the address from the CH351s
    auto address = Platform::readAddress();
    if constexpr (inDebugMode) {
        Serial.println(F("NEW TRANSACTION"));
        Serial.print(F("ADDRESS: 0x"));
        Serial.println(address, HEX);
    }
    if (Platform::isReadOperation()) {
        Platform::configureDataLinesForRead();
        if (Platform::isIOOperation()) {
            talkToi960<inDebugMode, true>(address, TreatAsInstruction{});
        } else {
            talkToi960<inDebugMode, true>(address, TreatAsOnChipAccess{});
        }
    } else {
        Platform::configureDataLinesForWrite();
        if (Platform::isIOOperation()) {
            talkToi960<inDebugMode, false>(address, TreatAsInstruction{});
        } else {
            talkToi960<inDebugMode, false>(address, TreatAsOnChipAccess{});
        }
    }
    if constexpr (inDebugMode) {
        Serial.println(F("END TRANSACTION"));
    }
    // need this delay to synchronize everything :)
    singleCycleDelay();
}

template<bool inDebugMode = true>
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
        constexpr auto BufferSize = 8192;
        auto previousBank = Platform::getBank(AccessFromIBUS{});
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            Platform::setBank(view, AccessFromIBUS{});
            auto* theBuffer = Platform::viewAreaAsBytes(view, AccessFromIBUS{});
            theFirmware.read(const_cast<uint8_t*>(theBuffer), BufferSize);
            Serial.print(F("."));
        }
        Serial.println(F("DONE!"));
        theFirmware.close();
        Platform::setBank(previousBank, AccessFromIBUS{});
    }
}
void 
setupPins() noexcept {
    // EnterDebugMode needs to be pulled low to start up in debug mode
    pinMode(Pin::EnterDebugMode, INPUT_PULLUP);
    // setup the IBUS bank
    DDRJ = 0xFF;
    PORTJ = 0x00;
}
void
setup() {
    setupPins();
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
template<bool ForceEnterDebugMode = true>
bool 
isDebuggingSession() noexcept {
    return ForceEnterDebugMode || digitalRead<Pin::EnterDebugMode>() == LOW;
}
void 
loop() {
    if (isDebuggingSession()) {
        while (true) {
            waitForDataState();
            handleTransaction<true>(SelectedLogic{});
        }
    } else {
        while (true) {
            waitForDataState();
            handleTransaction<false>(SelectedLogic{});
        }

    }
}


#ifdef INT7_vect
ISR(INT7_vect) {
    // since we are using PE7 as CLKO, this vector can be leveraged for some
    // sort of internal operation if desired
}
#endif

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

template<bool inDebugMode>
inline
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
inline
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
using InterfaceDataRegister = uint8_t*;
template<bool inDebugMode, bool isReadOperation>
[[gnu::noinline]]
inline
void
doCommunication(volatile SplitWord128& theView, InterfaceDataRegister lsb, InterfaceDataRegister dataLines) noexcept {
    // We do direct byte writes on AVR to accelerate the operations
    // we pass the pointers in as well to also prevent constant recomputation
    // of the destination or source addresses in extended memory.
    // 
    /// @todo eliminate the right shift by two using the byte offset instead of the view offset! Maintain word alignment
    // A SplitWord128 is actually a 16-byte block, so we could just view it as
    // that without any problems! What we would be doing is just masking out
    // the lower two bits and just returning the pointer 
    //
    // At the pointer we are here, instructions have already been interpreted
    // or have yet to be written to. This is a view change, nothing more. We
    // just want to reduce the amount of extra work required on each iteration
    // computing the current word. 
    for(;;) {
        // figure out which word we are currently looking at
        if constexpr (volatile auto& targetElement = theView[(*lsb & 0b1111) >> 2]; isReadOperation) {
            auto* theBytes = targetElement.bytes;
            dataLines[0] = theBytes[0];
            dataLines[1] = theBytes[1];
            dataLines[2] = theBytes[2];
            dataLines[3] = theBytes[3];
        } else {
            auto* theBytes = targetElement.bytes;
            // you must check each enable bit to see if you have to write to that byte
            // or not. You cannot just do a 32-bit write in all cases, this can
            // cause memory corruption pretty badly. 
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[0] = dataLines[0];
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[1] = dataLines[1];
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[2] = dataLines[2];
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[3] = dataLines[3];
            }
        }
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
    } 
}
template<bool inDebugMode, bool isReadOperation>
void
talkToi960(InterfaceDataRegister lsb, InterfaceDataRegister dataLines, const SplitWord32& addr, TreatAsInstruction) noexcept {
    if (inDebugMode) {
        Serial.println(F("INSTRUCTION REQUESTED!"));
    }
    //SplitWord32 addr{Platform::readAddress()};
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
            case 0xF0: // only group 0 is active right now
                performIOReadGroup0<inDebugMode>(operation);
                break;
            default:
                // just ignore the data and return what we have inside of
                // the operation object itself. 
                //
                // This greatly simplifies everything!
                break;
        }
    }
    doCommunication<inDebugMode, isReadOperation>(operation.args_, lsb, dataLines);
    if constexpr (!isReadOperation) {
        switch (operation.getGroup()) {
            case 0xF0:
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
talkToi960(InterfaceDataRegister lsb, InterfaceDataRegister dataLines, const SplitWord32& addr, T) noexcept {
    // only need to set this once, it is literally impossible for this to span
    // banks
    Platform::setBank(addr, typename T::AccessMethod{});
    doCommunication<inDebugMode, isReadOperation>(Platform::getTransactionWindow(addr, typename T::AccessMethod{}), lsb, dataLines);
}


template<bool inDebugMode>
void
handleTransaction(InterfaceDataRegister lsb, InterfaceDataRegister dataLines, LoadFromIBUS) noexcept {
    // first we need to extract the address from the CH351s
    if constexpr (inDebugMode) {
        Serial.println(F("NEW TRANSACTION"));
    }
    uint32_t* theAddr= reinterpret_cast<uint32_t*>(lsb);
    SplitWord32 addr{*theAddr};
    if (Platform::isWriteOperation()) {
        Platform::configureDataLinesForWrite();
        if (Platform::isIOOperation()) {
            talkToi960<inDebugMode, false>(lsb, dataLines, addr, TreatAsInstruction{});
        } else {
            talkToi960<inDebugMode, false>(lsb, dataLines, addr, TreatAsOnChipAccess{});
        }
    } else {
        Platform::configureDataLinesForRead();
        if (Platform::isIOOperation()) {
            talkToi960<inDebugMode, true>(lsb, dataLines, addr, TreatAsInstruction{});
        } else {
            talkToi960<inDebugMode, true>(lsb, dataLines, addr, TreatAsOnChipAccess{});
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
    pinMode(Pin::BE0, INPUT);
    pinMode(Pin::BE1, INPUT);
    pinMode(Pin::BE2, INPUT);
    pinMode(Pin::BE3, INPUT);
    pinMode(Pin::DEN, INPUT);
    pinMode(Pin::BLAST, INPUT);
    pinMode(Pin::WR, INPUT);
    if constexpr (MCUHasDirectAccess) {
        pinMode(Pin::READY, OUTPUT);
        digitalWrite<Pin::READY, HIGH>();
    }
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
template<bool ForceEnterDebugMode = EnableDebugMode>
bool 
isDebuggingSession() noexcept {
    return ForceEnterDebugMode || digitalRead<Pin::EnterDebugMode>() == LOW;
}
void 
loop() {
    InterfaceDataRegister addressLines = reinterpret_cast<uint8_t*>(0x2200);
    InterfaceDataRegister dataLines = reinterpret_cast<uint8_t*>(0x2208);
    InterfaceDataRegister controlSignals = reinterpret_cast<uint8_t*>(0x2210);
    InterfaceDataRegister bankRegister = reinterpret_cast<uint8_t*>(0x2218);
    if (isDebuggingSession()) {
        while (true) {
            waitForDataState();
            handleTransaction<true>(addressLines, dataLines, SelectedLogic{});
        }
    } else {
        while (true) {
            waitForDataState();
            handleTransaction<false>(addressLines, dataLines, SelectedLogic{});
        }

    }
}


#ifdef INT7_vect
ISR(INT7_vect) {
    // since we are using PE7 as CLKO, this vector can be leveraged for some
    // sort of internal operation if desired
}
#endif

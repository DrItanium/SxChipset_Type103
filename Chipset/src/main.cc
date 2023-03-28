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
using DataRegister8 = volatile uint8_t*;
using DataRegister32 = volatile uint32_t*;
template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
inline
void
doCommunication(volatile SplitWord128& theView, DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
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
        auto lowest = *addressLines & 0b1111;
        if constexpr (volatile auto& targetElement = theView[(lowest) >> 2]; isReadOperation) {
            auto* theBytes = targetElement.bytes;
            if constexpr (width == NativeBusWidth::Sixteen) {
                if ((lowest & 0b0010)) {
                    // upper half
                    dataLines[2] = theBytes[2];
                    dataLines[3] = theBytes[3];
                } else {
                    // lower half
                    dataLines[0] = theBytes[0];
                    dataLines[1] = theBytes[1];
                }
            } else {
                // in all other cases do the whole thing
                dataLines[0] = theBytes[0];
                dataLines[1] = theBytes[1];
                dataLines[2] = theBytes[2];
                dataLines[3] = theBytes[3];
            }
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
template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
void
talkToi960(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, TreatAsInstruction) noexcept {
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
    doCommunication<inDebugMode, isReadOperation, width>(operation.args_, addressLines, dataLines);
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


template<bool inDebugMode, bool isReadOperation, NativeBusWidth width, typename T>
void
talkToi960(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, T) noexcept {
    if constexpr (inDebugMode) {
        Serial.println(F("Direct memory access begin!"));
    }
    // only need to set this once, it is literally impossible for this to span
    // banks
    Platform::setBank(addr, typename T::AccessMethod{});
    doCommunication<inDebugMode, isReadOperation, width>(Platform::getTransactionWindow(addr, typename T::AccessMethod{}), 
            addressLines, dataLines);
    if constexpr (inDebugMode) {
        Serial.println(F("Direct memory access end!"));
    }
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
void
handleTransaction(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, LoadFromIBUS) noexcept {
        for (byte i = 4; i < 8; ++i) {
            if constexpr (isReadOperation) {
                dataLines[i] = 0xFF;
            } else {
                dataLines[i] = 0;
            }
        }
    if (addr.bytes[3] >= 0xF0) {
        talkToi960<inDebugMode, isReadOperation, width>(addressLines, dataLines, addr, TreatAsInstruction{});
    } else {
        talkToi960<inDebugMode, isReadOperation, width>(addressLines, dataLines, addr, TreatAsOnChipAccess{});
    }
}

template<bool inDebugMode, NativeBusWidth width>
void
handleTransaction(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, LoadFromIBUS) noexcept {
    // first we need to extract the address from the CH351s
    if constexpr (inDebugMode) {
        Serial.println(F("NEW TRANSACTION"));
    }
    if (Platform::isWriteOperation()) {
        handleTransaction<inDebugMode, false, width>(addressLines, dataLines, addr, LoadFromIBUS{});
    } else {
        handleTransaction<inDebugMode, true, width>(addressLines, dataLines, addr, LoadFromIBUS{});
    }
    if constexpr (inDebugMode) {
        Serial.println(F("END TRANSACTION"));
    }
    // need this delay to synchronize everything :)
    singleCycleDelay();
}

template<bool inDebugMode = true, uint32_t maxFileSize = 2048ul * 1024ul, auto BufferSize = 16384>
void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = maxFileSize;
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
    switch (Platform::getInstalledCPUKind()) {
        case CPUKind::Sx:
            Serial.println(F("i960Sx CPU detected!"));
            break;
        case CPUKind::Kx:
            Serial.println(F("i960Kx CPU detected!"));
            break;
        case CPUKind::Jx:
            Serial.println(F("i960Jx CPU detected!"));
            break;
        case CPUKind::Hx:
            Serial.println(F("i960Hx CPU detected!"));
            break;
        case CPUKind::Cx:
            Serial.println(F("i960Cx CPU detected!"));
            break;
        default:
            Serial.println(F("Unknown i960 cpu detected!"));
            break;
    }
    // find firmware.bin and install it into the 512k block of memory
    installMemoryImage();
    pullCPUOutOfReset();
}
template<bool ForceEnterDebugMode = EnableDebugMode>
bool 
isDebuggingSession() noexcept {
    return ForceEnterDebugMode || digitalRead<Pin::EnterDebugMode>() == LOW;
}
template<bool inDebugMode, NativeBusWidth width> 
[[noreturn]] 
void 
executionBody() noexcept {
    DataRegister8 AddressLinesPtr = reinterpret_cast<DataRegister8>(0x2200);
    DataRegister8 DataLinesPtr = reinterpret_cast<DataRegister8>(0x2208);
    DataRegister32 AddressLines32Ptr = reinterpret_cast<DataRegister32>(0x2200);
    while (true) {
        waitForDataState();
        //AddressStorage.full = *AddressLines32Ptr;
        handleTransaction<inDebugMode, width>(AddressLinesPtr, DataLinesPtr, SplitWord32{*AddressLines32Ptr}, SelectedLogic{});
    }
}
template<NativeBusWidth width>
void
discoveryDebugKindAndDispatch() {
    Serial.print(F("Chipset Debugging: "));
    if (isDebuggingSession()) {
        Serial.println(F("ENABLED"));
        executionBody<true, width>();
    } else {
        Serial.println(F("DISABLED"));
        executionBody<false, width>();
    }
}
void 
loop() {
    switch (getBusWidth(Platform::getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            Serial.println(F("16-bit bus width detected"));
            discoveryDebugKindAndDispatch<NativeBusWidth::Sixteen>();
            break;
        case NativeBusWidth::ThirtyTwo:
            Serial.println(F("32-bit bus width detected"));
            discoveryDebugKindAndDispatch<NativeBusWidth::ThirtyTwo>();
            break;
        default:
            Serial.println(F("Undefined bus width detected (fallback to 32-bit)"));
            discoveryDebugKindAndDispatch<NativeBusWidth::Unknown>();
            break;
    }
}


#ifdef INT7_vect
ISR(INT7_vect) {
    // since we are using PE7 as CLKO, this vector can be leveraged for some
    // sort of internal operation if desired
}
#endif

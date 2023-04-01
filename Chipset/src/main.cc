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
#include <SD.h>
#include "Types.h"
#include "Pinout.h"
#include "Peripheral.h"
#include "Setup.h"
#include "SerialDevice.h"
//#include "InfoDevice.h"
#include "TimerDevice.h"

SerialDevice theSerial;
//InfoDevice infoDevice;
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

using DataRegister8 = volatile uint8_t*;
using DataRegister32 = volatile uint32_t*;
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}
template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
struct CommunicationKernel {
    using Self = CommunicationKernel<inDebugMode, isReadOperation, width>;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

template<uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0>
static void 
doFixedCommunication(DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
    static constexpr SplitWord128 contents {
        static_cast<uint8_t>(a),
        static_cast<uint8_t>(a >> 8),
        static_cast<uint8_t>(a >> 16),
        static_cast<uint8_t>(a >> 24),
        static_cast<uint8_t>(b),
        static_cast<uint8_t>(b >> 8),
        static_cast<uint8_t>(b >> 16),
        static_cast<uint8_t>(b >> 24),
        static_cast<uint8_t>(c),
        static_cast<uint8_t>(c >> 8),
        static_cast<uint8_t>(c >> 16),
        static_cast<uint8_t>(c >> 24),
        static_cast<uint8_t>(d),
        static_cast<uint8_t>(d >> 8),
        static_cast<uint8_t>(d >> 16),
        static_cast<uint8_t>(d >> 24),
    };
    if constexpr (const uint8_t* theBytes = &contents.bytes[getWordByteOffset(*addressLines)]; isReadOperation) {
        // in all other cases do the whole thing
        dataLines[0] = theBytes[0];
        dataLines[1] = theBytes[1];
        dataLines[2] = theBytes[2];
        dataLines[3] = theBytes[3];
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[4];
        dataLines[1] = theBytes[5];
        dataLines[2] = theBytes[6];
        dataLines[3] = theBytes[7];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[8];
        dataLines[1] = theBytes[9];
        dataLines[2] = theBytes[10];
        dataLines[3] = theBytes[11];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[12];
        dataLines[1] = theBytes[13];
        dataLines[2] = theBytes[14];
        dataLines[3] = theBytes[15];
        /// @todo do not sample blast at the end of a 16-byte transaction
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    } else {
        // just ignore since the fixed values make no sense
        singleCycleDelay();
        singleCycleDelay();
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    }
}
static void
doCommunication(volatile SplitWord128& theView, DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
    if constexpr (DataRegister8 theBytes = &theView.bytes[getWordByteOffset(*addressLines)]; isReadOperation) {
        // in all other cases do the whole thing
        dataLines[0] = theBytes[0];
        dataLines[1] = theBytes[1];
        dataLines[2] = theBytes[2];
        dataLines[3] = theBytes[3];
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[4];
        dataLines[1] = theBytes[5];
        dataLines[2] = theBytes[6];
        dataLines[3] = theBytes[7];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[8];
        dataLines[1] = theBytes[9];
        dataLines[2] = theBytes[10];
        dataLines[3] = theBytes[11];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();

        dataLines[0] = theBytes[12];
        dataLines[1] = theBytes[13];
        dataLines[2] = theBytes[14];
        dataLines[3] = theBytes[15];
        /// @todo do not sample blast at the end of a 16-byte transaction
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    } else {
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
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        if (digitalRead<Pin::BE0>() == LOW) {
            theBytes[4] = dataLines[0];
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            theBytes[5] = dataLines[1];
        }
        if (digitalRead<Pin::BE2>() == LOW) {
            theBytes[6] = dataLines[2];
        }
        if (digitalRead<Pin::BE3>() == LOW) {
            theBytes[7] = dataLines[3];
        }
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        if (digitalRead<Pin::BE0>() == LOW) {
            theBytes[8] = dataLines[0];
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            theBytes[9] = dataLines[1];
        }
        if (digitalRead<Pin::BE2>() == LOW) {
            theBytes[10] = dataLines[2];
        }
        if (digitalRead<Pin::BE3>() == LOW) {
            theBytes[11] = dataLines[3];
        }
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        if (digitalRead<Pin::BE0>() == LOW) {
            theBytes[12] = dataLines[0];
        }
        if (digitalRead<Pin::BE1>() == LOW) {
            theBytes[13] = dataLines[1];
        }
        if (digitalRead<Pin::BE2>() == LOW) {
            theBytes[14] = dataLines[2];
        }
        if (digitalRead<Pin::BE3>() == LOW) {
            theBytes[15] = dataLines[3];
        }
        /// @todo do not sample blast at the end of a 16-byte transaction
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    }
}
};

template<bool inDebugMode, bool isReadOperation>
struct CommunicationKernel<inDebugMode, isReadOperation, NativeBusWidth::Sixteen> {
    using Self = CommunicationKernel<inDebugMode, isReadOperation, NativeBusWidth::Sixteen>;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
private:
    template<uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0>
    static void doFixedReadOperation(DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
        static constexpr SplitWord128 contents{
            static_cast<uint8_t>(a),
                static_cast<uint8_t>(a >> 8),
                static_cast<uint8_t>(a >> 16),
                static_cast<uint8_t>(a >> 24),
                static_cast<uint8_t>(b),
                static_cast<uint8_t>(b >> 8),
                static_cast<uint8_t>(b >> 16),
                static_cast<uint8_t>(b >> 24),
                static_cast<uint8_t>(c),
                static_cast<uint8_t>(c >> 8),
                static_cast<uint8_t>(c >> 16),
                static_cast<uint8_t>(c >> 24),
                static_cast<uint8_t>(d),
                static_cast<uint8_t>(d >> 8),
                static_cast<uint8_t>(d >> 16),
                static_cast<uint8_t>(d >> 24),
        };
        // just skip over the lowest 16-bits if we start unaligned
        uint8_t value = *addressLines;
        const uint8_t* theBytes = &contents.bytes[getWordByteOffset(*addressLines)];
        if ((value & 0b0010) == 0) {
            dataLines[0] = theBytes[0];
            dataLines[1] = theBytes[1];
            auto end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
        }
        // put in some amount of wait states before just signalling again
        // since we started on the lower half of a 32-bit word
        dataLines[2] = theBytes[2];
        dataLines[3] = theBytes[3];
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        dataLines[0] = theBytes[4];
        dataLines[1] = theBytes[5];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        // put in some amount of wait states before just signalling again
        // since we started on the lower half of a 32-bit word
        dataLines[2] = theBytes[6];
        dataLines[3] = theBytes[7];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        dataLines[0] = theBytes[8];
        dataLines[1] = theBytes[9];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        // put in some amount of wait states before just signalling again
        // since we started on the lower half of a 32-bit word
        dataLines[2] = theBytes[10];
        dataLines[3] = theBytes[11];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        dataLines[0] = theBytes[12];
        dataLines[1] = theBytes[13];
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        // put in some amount of wait states before just signalling again
        // since we started on the lower half of a 32-bit word
        dataLines[2] = theBytes[14];
        dataLines[3] = theBytes[15];
        /// @todo do not sample blast at the end of a 16-byte transaction
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    }
    static void doFixedWriteOperation() noexcept {
        singleCycleDelay();
        singleCycleDelay();
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        // upper half
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        // upper half
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        // upper half
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
        singleCycleDelay();
        singleCycleDelay();
        // upper half
        end = Platform::isBurstLast();
        signalReady();
        if (end) {
            return;
        }
    }
    static void doReadOperation(volatile SplitWord128& theView, DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
        //for(;;) {
            // just skip over the lowest 16-bits if we start unaligned
            uint8_t value = *addressLines;
            DataRegister8 theBytes = &theView.bytes[getWordByteOffset(value)];
            if ((value & 0b0010) == 0) {
                dataLines[0] = theBytes[0];
                dataLines[1] = theBytes[1];
                auto end = Platform::isBurstLast();
                signalReady();
                if (end) {
                    return;
                }
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            dataLines[2] = theBytes[2];
            dataLines[3] = theBytes[3];
            auto end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            dataLines[0] = theBytes[4];
            dataLines[1] = theBytes[5];
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            dataLines[2] = theBytes[6];
            dataLines[3] = theBytes[7];
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            dataLines[0] = theBytes[8];
            dataLines[1] = theBytes[9];
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            dataLines[2] = theBytes[10];
            dataLines[3] = theBytes[11];
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            dataLines[0] = theBytes[12];
            dataLines[1] = theBytes[13];
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            dataLines[2] = theBytes[14];
            dataLines[3] = theBytes[15];
            /// @todo do not sample blast at the end of a 16-byte transaction
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
        //} 
    }
    static void doWriteOperation(volatile SplitWord128& theView, DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
        // now we are aligned so start execution as needed
        //for(;;) {
            // figure out which word we are currently looking at
            uint8_t value = *addressLines;
            DataRegister8 theBytes = &theView.bytes[getWordByteOffset(value)]; 
            // if we are aligned to 32-bit word boundaries then just assume we
            // are at the start of the 16-byte block (the processor will stop
            // when it doesn't need data anymore). If we are not then skip over
            // this first two bytes and start at the upper two bytes of the
            // current word.
            //
            // I am also exploiting the fact that the processor can only ever
            // accept up to 16-bytes at a time if it is aligned to 16-byte
            // boundaries. If it is unaligned then the operation is broken up
            // into multiple transactions within the i960 itself. So yes, this
            // code will go out of bounds but it doesn't matter because the
            // processor will never go out of bounds.
            if ((value & 0b0010) == 0) {
                // lower half
                if (digitalRead<Pin::BE0>() == LOW) {
                    theBytes[0] = dataLines[0];
                }
                if (digitalRead<Pin::BE1>() == LOW) {
                    theBytes[1] = dataLines[1];
                }
                auto end = Platform::isBurstLast();
                signalReady();
                if (end) {
                    return;
                }
            }
            singleCycleDelay();
            singleCycleDelay();
            // upper half
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[2] = dataLines[2];
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[3] = dataLines[3];
            }
            auto end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[4] = dataLines[0];
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[5] = dataLines[1];
            }
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            // upper half
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[6] = dataLines[2];
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[7] = dataLines[3];
            }
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[8] = dataLines[0];
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[9] = dataLines[1];
            }
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            // upper half
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[10] = dataLines[2];
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[11] = dataLines[3];
            }
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[12] = dataLines[0];
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[13] = dataLines[1];
            }
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            // upper half
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[14] = dataLines[2];
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[15] = dataLines[3];
            }
            /// @todo do not sample blast at the end of a 16-byte transaction
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // we should never get here as transactions are at most 16 bytes in
            // size. However, if we did get here then we would just start the
            // transaction again at the top!
        //}
    }

public:
    template<uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0>
    static void
    doFixedCommunication(DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
        /// @todo check the start position as that will describe the cycle shape
        if constexpr (isReadOperation) {
            doFixedReadOperation<a, b, c, d>(addressLines, dataLines);
        } else {
            // a fixed write operation doesn't actually care about updating
            // anything since it doesn't make sense at all
            doFixedWriteOperation();
        }
    }
    static void
    doCommunication(volatile SplitWord128& theView, DataRegister8 addressLines, DataRegister8 dataLines) noexcept {
        /// @todo check the start position as that will describe the cycle shape
        if constexpr (isReadOperation) {
            if constexpr (inDebugMode) {
                Serial.println(F("Starting Read Operation Proper"));
            }
            doReadOperation(theView, addressLines, dataLines);
            if constexpr (inDebugMode) {
                Serial.println(F("Ending Read Operation Proper"));
            }
        } else {
            if constexpr (inDebugMode) {
                Serial.println(F("Starting Write Operation Proper"));
            }
            doWriteOperation(theView, addressLines, dataLines);
            if constexpr (inDebugMode) {
                Serial.println(F("Ending Write Operation Proper"));
            }
        }
    }
};

BeginDeviceOperationsList(InfoDevice)
    GetChipsetClock,
    GetCPUClock,
EndDeviceOperationsList(InfoDevice)

template<bool inDebugMode, NativeBusWidth width>
inline
void
performIOReadGroup0(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 opcode, SplitWord128& body) noexcept {
    // unlike standard i960 operations, we only encode the data we actually care
    // about out of the packet when performing a read operation so at this
    // point it doesn't matter what kind of data the i960 is requesting.
    // This maintains consistency and makes the implementation much simpler
    switch (opcode.getIODevice<TargetPeripheral>()) {
        case TargetPeripheral::Info:
            switch (opcode.getIOFunction<InfoDeviceOperations>()) {
                case InfoDeviceOperations::Available:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0xFFFF'FFFF>(addressLines, dataLines);
                    break;
                case InfoDeviceOperations::Size:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<static_cast<uint8_t>(InfoDeviceOperations::Count)>(addressLines, dataLines);
                    break;
                case InfoDeviceOperations::GetChipsetClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU>(addressLines, dataLines);
                    break;
                case InfoDeviceOperations::GetCPUClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU/2>(addressLines, dataLines);
                    break;
                default:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(addressLines, dataLines);
                    // unknown device so do not do anything
                    break;
            }
            return;
        case TargetPeripheral::Serial:
            theSerial.performRead(opcode, body);
            CommunicationKernel<inDebugMode, true, width>::doCommunication(body, addressLines, dataLines);
            break;
        case TargetPeripheral::Timer:
            timerInterface.performRead(opcode, body);
            CommunicationKernel<inDebugMode, true, width>::doCommunication(body, addressLines, dataLines);
            break;
        default:
            CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(addressLines, dataLines);
            // unknown device so do not do anything
            break;
    }
}
template<bool inDebugMode, NativeBusWidth width>
inline
void
performIOWriteGroup0(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 opcode, SplitWord128& body) noexcept {
    // unlike standard i960 operations, we only decode the data we actually care
    // about out of the packet when performing a write operation so at this
    // point it doesn't matter what kind of data we were actually given
    switch (opcode.getIODevice<TargetPeripheral>()) {
        case TargetPeripheral::Serial:
            CommunicationKernel<inDebugMode, false, width>::doCommunication(body, addressLines, dataLines);
            theSerial.performWrite(opcode, body);
            break;
        case TargetPeripheral::Timer:
            CommunicationKernel<inDebugMode, false, width>::doCommunication(body, addressLines, dataLines);
            timerInterface.performWrite(opcode, body);
            break;
        default:
            CommunicationKernel<inDebugMode, false, width>::template doFixedCommunication<0>(addressLines, dataLines);
            // unknown device so do not do anything
            break;
    }
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
inline 
void
performIOGroup0Operation(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 opcode) noexcept {
    SplitWord128 operation;
    if constexpr (isReadOperation) {
        performIOReadGroup0<inDebugMode, width>(addressLines, dataLines, opcode, operation);
    } else {
        performIOWriteGroup0<inDebugMode, width>(addressLines, dataLines, opcode, operation);
    }
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
void
talkToi960(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, TreatAsInstruction) noexcept {
    if (inDebugMode) {
        Serial.println(F("INSTRUCTION REQUESTED!"));
    }
    // If the lowest four bits are not zero then that is a problem on the side
    // of the i960. For example, if you started in the middle of the 16-byte
    // block then the data will still be valid just not what you wanted. This
    // ignores a huge class of problems. Instead, we just process
    switch (addr.getIOGroup()) {
        case 0xF0: // only group 0 is active right now
            performIOGroup0Operation<inDebugMode, isReadOperation, width>(addressLines, dataLines, addr);
            break;
        default:
            // just return zeroes
            CommunicationKernel<inDebugMode, isReadOperation, width>::template doFixedCommunication<0>(addressLines, dataLines);
            break;
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
    CommunicationKernel<inDebugMode, isReadOperation, width>::doCommunication(Platform::getTransactionWindow(addr, typename T::AccessMethod{}), 
            addressLines, dataLines);
    if constexpr (inDebugMode) {
        Serial.println(F("Direct memory access end!"));
    }
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
void
handleTransaction(DataRegister8 addressLines, DataRegister8 dataLines, const SplitWord32 addr, LoadFromIBUS) noexcept {
    static constexpr uint8_t targetDirection = isReadOperation ? 0xFF : 0;
    for (byte i = 4; i < 8; ++i) {
        dataLines[i] = targetDirection;
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
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    DataRegister8 AddressLinesPtr = reinterpret_cast<DataRegister8>(0x2200);
    DataRegister8 DataLinesPtr = reinterpret_cast<DataRegister8>(0x2208);
    DataRegister32 AddressLines32Ptr = reinterpret_cast<DataRegister32>(0x2200);
    while (true) {
        waitForDataState();
        handleTransaction<inDebugMode, width>(AddressLinesPtr, DataLinesPtr, SplitWord32{*AddressLines32Ptr}, SelectedLogic{});
        singleCycleDelay();
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

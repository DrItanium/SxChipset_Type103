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
#include <Wire.h>

#include "Types.h"
#include "Pinout.h"
#include "Peripheral.h"
#include "Setup.h"
#include "SerialDevice.h"
//#include "InfoDevice.h"
#include "TimerDevice.h"
#include "DisplayPeripheral.h"

SerialDevice theSerial;
TimerDevice timerInterface;
DisplayInterface theDisplay;
struct IAC {
    uint16_t field2;
    uint8_t field1;
    uint8_t messageType;
    uint32_t field3;
    uint32_t field4;
    uint32_t field5;
};
volatile IAC iac_;

[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    Platform::signalReady();
}
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
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}
[[gnu::address(0x2200)]] volatile uint8_t addressLines[8];
[[gnu::address(0x2208)]] volatile uint8_t dataLines[4];
[[gnu::address(0x2208)]] volatile uint32_t dataLinesFull;
[[gnu::address(0x2208)]] volatile uint16_t dataLinesHalves[2];
[[gnu::address(0x220C)]] volatile uint32_t dataLinesDirection;
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_bytes[4];
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_LSB;

[[gnu::address(0x2200)]] volatile uint16_t AddressLines16Ptr[4];
[[gnu::address(0x2200)]] volatile uint32_t AddressLines32Ptr[2];
[[gnu::address(0x2200)]] volatile uint32_t addressLinesValue32;

/**
 * @brief Just go through the motions of a write operation but do not capture
 * anything being sent by the i960
 */
[[gnu::always_inline]]
inline void 
idleTransaction() noexcept {
    // just keep going until we are done
    do {
        insertCustomNopCount<4>();
        auto end = Platform::isBurstLast();
        signalReady();
        if (end) {
            break;
        }
    } while (true);
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

template<uint32_t a, uint32_t b = a, uint32_t c = a, uint32_t d = a>
[[gnu::always_inline]]
inline
static void 
doFixedCommunication(uint8_t lowest) noexcept {
    // we cannot hardcode the positions because we have no idea what the
    // offset is. So we have to do it this way, it is slower but it also means
    // the code is very straightforward
    if constexpr (isReadOperation) {
        if constexpr (a == b && b == c && c == d) {
            dataLinesFull = a;
            idleTransaction();
        } else {
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
            const uint8_t* theBytes = &contents.bytes[getWordByteOffset(lowest)]; 
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
            signalReady();
        }
    } else {
        idleTransaction();
    }
}
[[gnu::always_inline]]
inline
static void
doCommunication(DataRegister8 theBytes, uint8_t lowest) noexcept {
    if constexpr (isReadOperation) {
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
        // do not sample blast at the end of a 16-byte transaction
        signalReady();
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
        // don't sample blast at the end since we know we will be done
        signalReady();
    }
}
    [[gnu::always_inline]]
    inline
    static void
    doCommunication(volatile SplitWord128& body, uint8_t lowest) noexcept {
        doCommunication(&body.bytes[getWordByteOffset(lowest)], lowest);
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
    template<uint32_t a, uint32_t b = a, uint32_t c = a, uint32_t d = a>
    [[gnu::always_inline]]
    inline
    static void doFixedReadOperation(uint8_t lowest) noexcept {
        if constexpr (a == b && b == c && c == d) {
            dataLinesFull = a;
            idleTransaction();
        } else {
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
            uint8_t value = lowest;
            uint8_t offset = getWordByteOffset(value);
            const uint8_t* theBytes = &contents.bytes[offset];
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
            // don't sample at the end!
            signalReady();
        }
    }

public:
    template<uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0>
    [[gnu::always_inline]]
    inline
    static void
    doFixedCommunication(uint8_t lowest) noexcept {
        /// @todo check the start position as that will describe the cycle shape
        if constexpr (isReadOperation) {
            doFixedReadOperation<a, b, c, d>(lowest);
        } else {
            idleTransaction();
        }
    }
    [[gnu::always_inline]]
    inline
    static void
    doCommunication(DataRegister8 theBytes, uint8_t lowest) noexcept {
        do {
            // figure out which word we are currently looking at
            uint8_t value = lowest;
            //DataRegister8 theBytes = &theView.bytes[getWordByteOffset(value)];
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


            // The later field is used to denote if the given part of the
            // transaction is later on in burst. If it is then we will
            // terminate early without evaluating BLAST if the upper byte
            // enable is high. This is because if we hit 0b1001 this would be
            // broken up into two 16-bit values (0b1101 and 0b1011) which is
            // fine but in all cases the first 1 we encounter after finding the
            // first zero in the byte enable bits we are going to terminate
            // anyway. So don't waste time evaluating BLAST at all!
#define X(dli, d0, b0, d1, b1, later) \
            { \
                if constexpr (isReadOperation) { \
                    dataLines[d0] = theBytes[b0]; \
                    dataLines[d1] = theBytes[b1]; \
                } else { \
                    if constexpr (later) { \
                        /* in this case, we will immediately terminate if the 
                         * upper byte enable bit is 1
                         *
                         * Also, since this is later on in the process, it
                         * should be safe to just propagate without performing
                         * the check itself
                         */ \
                        theBytes[b0] = dataLines[d0]; \
                        if (digitalRead<Pin:: BE ## d1 > () == LOW) { \
                            theBytes[b1] = dataLines[d1]; \
                        } else { \
                            break; \
                        } \
                    } else { \
                        /*
                         * First time through, we want to actually check the
                         * upper byte enable bit first and if it is then
                         * check the lower bits
                         */ \
                        if (digitalRead<Pin:: BE ## d1 >() == LOW) { \
                            theBytes[b1] = dataLines[d1]; \
                            if (digitalRead<Pin:: BE ## d0 > () == LOW) { \
                                theBytes[b0] = dataLines[d0]; \
                            } \
                        } else { \
                            /*
                             * In this case, we know that the lower 8-bits are
                             * the starting location and the ending location so
                             * assign and then break out!
                             * 
                             * The only way we get here is if the upper byte
                             * enable bit is high!
                             */ \
                            theBytes[b0] = dataLines[d0]; \
                            break; \
                        } \
                    } \
                } \
                if constexpr (b0 != 14 && b1 != 15) { \
                    if (Platform::isBurstLast()) { \
                        break; \
                    } \
                    signalReady(); \
                    if constexpr (!isReadOperation) { \
                        insertCustomNopCount<4>(); /* The delay for the ready signal */ \
                    } \
                } \
            }
            if ((value & 0b0010) == 0) {
                X(0,0,0,1,1, false);
            }
            X(1,2,2,3,3, false);
            X(0,0,4,1,5, true);
            X(1,2,6,3,7, true);
            X(0,0,8,1,9, true);
            X(1,2,10,3,11, true);
            X(0,0,12,1,13, true);
            X(1,2,14,3,15, true);
#undef X
        } while (false);
        signalReady();
    }


    [[gnu::always_inline]]
    inline
    static void
    doCommunication(volatile SplitWord128& body, uint8_t lowest) noexcept {
        doCommunication(&body.bytes[getWordByteOffset(lowest)], lowest);
    }
};
BeginDeviceOperationsList(InfoDevice)
    GetChipsetClock,
    GetCPUClock,
    IAC,
EndDeviceOperationsList(InfoDevice)
ConnectPeripheral(TargetPeripheral::Info, InfoDeviceOperations);




template<bool inDebugMode, bool isReadOperation, NativeBusWidth width, TargetPeripheral p>
[[gnu::always_inline]]
inline
void sendOpcodeSize(uint8_t offset) noexcept {
    CommunicationKernel<inDebugMode, isReadOperation, width>::template doFixedCommunication<OpcodeCount_v<p>>(offset);
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
[[gnu::always_inline]]
inline
void sendZero(uint8_t offset) noexcept {
    CommunicationKernel<inDebugMode, isReadOperation, width>::template doFixedCommunication<0>(offset);
}

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width>
[[gnu::always_inline]]
inline
void sendBoolean(bool value, uint8_t offset) noexcept {
    if (value) {
        CommunicationKernel<inDebugMode, isReadOperation, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
    } else {
        sendZero<inDebugMode, isReadOperation, width>(offset);
    }
}


template<bool inDebugMode, NativeBusWidth width>
[[gnu::always_inline]]
inline
void
performIOReadGroup0(SplitWord128& body, uint8_t group, uint8_t function, uint8_t offset) noexcept {
    // unlike standard i960 operations, we only encode the data we actually care
    // about out of the packet when performing a read operation so at this
    // point it doesn't matter what kind of data the i960 is requesting.
    // This maintains consistency and makes the implementation much simpler
    switch (static_cast<TargetPeripheral>(group)) {
        case TargetPeripheral::Info:
            switch (getFunctionCode<TargetPeripheral::Info>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Info>;
                case K::Available:
                    sendBoolean<inDebugMode, true, width>(true, offset);
                    return;
                case K::Size:
                    sendOpcodeSize<inDebugMode, true, width, TargetPeripheral::Info>(offset);
                    return;
                case K::GetChipsetClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU>(offset);
                    return;
                case K::GetCPUClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU/2>(offset);
                    return;
                case K::IAC:
                    body[0].halves[0] = iac_.field2;
                    body[0].bytes[2] = iac_.field1;
                    body[0].bytes[3] = iac_.messageType;
                    body[1].full = iac_.field3;
                    body[2].full = iac_.field4;
                    body[3].full = iac_.field5;
                    break;
                default:
                    sendZero<inDebugMode, true, width>(offset);
                    return;
            }
            return;
        case TargetPeripheral::Serial:
            switch (getFunctionCode<TargetPeripheral::Serial>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Serial>;
                case K::Available:
                    sendBoolean<inDebugMode, true, width>(theSerial.isAvailable(), offset);
                    return;
                case K::Size:
                    sendOpcodeSize<inDebugMode, true, width, TargetPeripheral::Serial>(offset);
                    return;
                case K::RW:
                    body[0].halves[0] = Serial.read();
                    break;
                case K::Flush:
                    Serial.flush();
                    break;
                case K::Baud:
                    body[0].full = theSerial.getBaudRate();
                    break;
                default:
                    sendZero<inDebugMode, true, width>(offset);
                    return;
            }
            break;
        case TargetPeripheral::Timer:
            switch (getFunctionCode<TargetPeripheral::Timer>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Timer>;
                case K::Available:
                    sendBoolean<inDebugMode, true, width>(timerInterface.isAvailable(), offset);
                    return;
                case K::Size:
                    sendOpcodeSize<inDebugMode, true, width, TargetPeripheral::Timer>(offset);
                    return;
                case K::SystemTimerPrescalar:
                    body.bytes[0] = timerInterface.getSystemTimerPrescalar();
                    break;
                case K::SystemTimerComparisonValue:
                    body.bytes[0] = timerInterface.getSystemTimerComparisonValue();
                    break;
                default:
                    sendZero<inDebugMode, true, width>(offset);
                    return;

            }
            break;
        case TargetPeripheral::Display:
            theDisplay.handleReadOperations(body, function, offset);
            break;
        default:
            break;
    }
    CommunicationKernel<inDebugMode, true, width>::doCommunication(body, offset);
}
template<bool inDebugMode, NativeBusWidth width>
[[gnu::always_inline]]
inline
void
performIOWriteGroup0(SplitWord128& body, uint8_t group, uint8_t function, uint8_t offset) noexcept {
    // unlike standard i960 operations, we only decode the data we actually care
    // about out of the packet when performing a write operation so at this
    // point it doesn't matter what kind of data we were actually given
    //
    // need to sample the address lines prior to grabbing data off the bus
    CommunicationKernel<inDebugMode, false, width>::doCommunication(body, offset);
    asm volatile ("nop");
    switch (static_cast<TargetPeripheral>(group)) {
        case TargetPeripheral::Serial:
            theSerial.handleWriteOperations(body, function, offset);
            break;
        case TargetPeripheral::Timer:
            timerInterface.handleWriteOperations(body, function, offset);
            break;
        case TargetPeripheral::Display:
            theDisplay.handleWriteOperations(body, function, offset);
            break;
        default:
            // unknown device so do not do anything
            break;
    }
}

uint8_t computeBankIndex(uint8_t lower, uint8_t upper, typename TreatAsOnChipAccess::AccessMethod) noexcept {
#ifndef __BUILTIN_AVR_INSERT_BITS
        uint8_t a = static_cast<uint8_t>(bytes[1] >> 6) & 0b11;
        uint8_t b = static_cast<uint8_t>(bytes[2] << 2) & 0b1111'1100;
        return a + b;
#else
        return __builtin_avr_insert_bits(0xffffff76, lower, 
                __builtin_avr_insert_bits(0x543210ff, upper, 0));
#endif
}
inline
uint8_t computeBankIndex(uint32_t address, typename TreatAsOnChipAccess::AccessMethod) noexcept {
    return computeBankIndex(static_cast<uint8_t>(address >> 8),
            static_cast<uint8_t>(address >> 16),
            typename TreatAsOnChipAccess::AccessMethod{});
}
template<uint16_t sectionMask, uint16_t offsetMask>
constexpr
uint16_t
computeTransactionWindow_Generic(uint16_t offset) noexcept {
    return sectionMask | (offset & offsetMask);
}
constexpr
uint16_t 
computeTransactionWindow(uint16_t offset, typename TreatAsOnChipAccess::AccessMethod) noexcept {
    return computeTransactionWindow_Generic<0x4000, 0x3ffc>(offset);
}
constexpr
uint16_t 
computeTransactionWindow(uint16_t offset, typename TreatAsOffChipAccess::AccessMethod) noexcept {
    return computeTransactionWindow_Generic<0x8000, 0x7ffc>(offset);
}

template<typename T>
DataRegister8
getTransactionWindow(uint16_t offset, T) noexcept {
    return memoryPointer<uint8_t>(computeTransactionWindow(offset, T{}));
}

template<bool i960DirectlyControlsIBUSBank>
[[gnu::always_inline]]
inline
void 
updateBank(uint32_t addr, typename TreatAsOnChipAccess::AccessMethod) noexcept {
    if constexpr (!i960DirectlyControlsIBUSBank) {
        Platform::setBank(computeBankIndex(addr, typename TreatAsOnChipAccess::AccessMethod {}),
                typename TreatAsOnChipAccess::AccessMethod{});
    }
}
inline 
void 
updateDataLinesDirection(uint8_t value) noexcept {
    dataLinesDirection_bytes[0] = value;
    dataLinesDirection_bytes[1] = value;
    dataLinesDirection_bytes[2] = value;
    dataLinesDirection_bytes[3] = value;
}

template<bool inDebugMode, bool i960DirectlyControlsIBUSBank, NativeBusWidth width> 
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    SplitWord128 operation;
    uint8_t currentDirection = dataLinesDirection_LSB;
    if constexpr (i960DirectlyControlsIBUSBank) {
        DDRJ = 0;
    } 
    // disable pullups!
    Platform::setBank(0, typename TreatAsOnChipAccess::AccessMethod{});
    Platform::setBank(0, typename TreatAsOffChipAccess::AccessMethod{});
    while (true) {
        waitForDataState();
        if constexpr (inDebugMode) {
            Serial.println(F("NEW TRANSACTION"));
        }
        startTransaction();
        uint32_t al = addressLinesValue32;
        /// @todo figure out the best way to only update the Bank index when needed
        if (auto majorCode = static_cast<uint8_t>(al >> 24), offset = static_cast<uint8_t>(al); Platform::isWriteOperation()) {
            if (currentDirection) {
                currentDirection = 0;
                updateDataLinesDirection(currentDirection);
            }
            switch (majorCode) {
                case 0x00: 
                    updateBank<i960DirectlyControlsIBUSBank>(al, typename TreatAsOnChipAccess::AccessMethod{});
                    CommunicationKernel<inDebugMode, false, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOnChipAccess::AccessMethod{}),
                            offset
                            );
                    break;


                case 0xF0:
                    performIOWriteGroup0<inDebugMode, width>(operation, 
                            static_cast<uint8_t>(al >> 16),
                            static_cast<uint8_t>(al >> 8),
                            offset);
                    break;
                case 0xF1: case 0xF2: case 0xF3: 
                case 0xF4: case 0xF5: case 0xF6: case 0xF7: 
                case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                    CommunicationKernel<inDebugMode, false, width>::template doFixedCommunication<0>(offset);
                    break;
                default: 
                    Platform::setBank(al,
                            typename TreatAsOffChipAccess::AccessMethod{});
                    CommunicationKernel<inDebugMode, false, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOffChipAccess::AccessMethod{}),
                            offset
                            );
                    break;
            }
        } else {
            if (!currentDirection) {
                currentDirection = 0xFF;
                updateDataLinesDirection(currentDirection);
            }
            switch (majorCode) {
                case 0x00: 
                    updateBank<i960DirectlyControlsIBUSBank>(al, typename TreatAsOnChipAccess::AccessMethod{});
                    CommunicationKernel<inDebugMode, true, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOnChipAccess::AccessMethod{}),
                            offset
                            );
                    break;
                case 0xF0:
                    performIOReadGroup0<inDebugMode, width>(operation, 
                            static_cast<uint8_t>(al >> 16),
                            static_cast<uint8_t>(al >> 8),
                            offset);
                    break;
                case 0xF1: case 0xF2: case 0xF3:
                case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(offset);
                    break;
                default:
                    Platform::setBank(al,
                            typename TreatAsOffChipAccess::AccessMethod{});
                    CommunicationKernel<inDebugMode, true, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOffChipAccess::AccessMethod{}),
                            offset
                            );
                    break;
            }
        }
        endTransaction();
        if constexpr (inDebugMode) {
            Serial.println(F("END TRANSACTION"));
        }
        singleCycleDelay();
    }
}

template<bool inDebugMode = true, uint32_t maxFileSize = 1024ul * 1024ul, auto BufferSize = 16384>
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
        Serial.println(F("The firmware image is too large to fit in sram!"));
        while (true) {
            delay(1000);
        }
    } else {
        auto previousBank = Platform::getBank(AccessFromIBUS{});
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            // just modify the bank as we go along
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
    pinMode(Pin::I960ControlsIBUSBank, INPUT_PULLUP);
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
#ifdef PHASE_DETECT_BEHAVIOR
    pinMode(Pin::TransactionDetect, OUTPUT);
    digitalWrite<Pin::TransactionDetect, HIGH>();
#endif
    if constexpr (MCUHasDirectAccess) {
        pinMode(Pin::READY, OUTPUT);
        digitalWrite<Pin::READY, HIGH>();
    }
}
void
setupDisplay() noexcept {
    theDisplay.begin();
}
void
setup() {
    setupPins();
    theSerial.begin();
    timerInterface.begin();
    Wire.begin();
    SPI.begin();
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    setupDisplay();
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
bool
beInDebugModeThisSession() noexcept {
    return digitalRead<Pin::EnterDebugMode>() == LOW;
}
template<bool ForceEnterDebugMode = EnableDebugMode>
bool 
isDebuggingSession() noexcept {
    if constexpr (IncludeDebugModeInFirmware) {
        return ForceEnterDebugMode || beInDebugModeThisSession();
    } else {
        return false;
    }
}
template<bool ForceI960AccessMode = Enablei960DirectlyControlsIBUSBank>
bool 
i960ControllingIBUSBankThisSession() noexcept {
    return ForceI960AccessMode || digitalRead<Pin::I960ControlsIBUSBank>() == LOW;
}
template<NativeBusWidth width>
void
discoveryDebugKindAndDispatch() {
    Serial.print(F("Chipset Debugging: "));
    if (isDebuggingSession()) {
        Serial.println(F("ENABLED"));
        Serial.print(F("IBUS is controlled by: "));
        if (i960ControllingIBUSBankThisSession()) {
            Serial.println(F("i960"));
            executionBody<true, true, width>();
        } else {
            Serial.println(F("AVR's PORTJ"));
            executionBody<true, false, width>();
        }
    } else {
        Serial.println(F("DISABLED"));
        Serial.print(F("IBUS is controlled by: "));
        if (i960ControllingIBUSBankThisSession()) {
            Serial.println(F("i960"));
            executionBody<false, true, width>();
        } else {
            Serial.println(F("AVR's PORTJ"));
            executionBody<false, false, width>();
        }
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



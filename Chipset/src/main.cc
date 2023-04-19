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
template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    if constexpr (index < 4) {
        if constexpr (index == 0) {
            if constexpr (EnablePortKCapture_D0_7) {
                PORTK = value;
            } 
        } 
        dataLines[index] = value;
    }
}
template<uint8_t index>
inline uint8_t getDataByte() noexcept {
    if constexpr (index < 4) {
        if constexpr (index == 0) {
            if constexpr (EnablePortKCapture_D0_7) {
                return PINK;
            } 
        } 
        return dataLines[index];
    }
}
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
template<bool isReadOperation, NativeBusWidth width>
struct CommunicationKernel {
    using Self = CommunicationKernel<isReadOperation, width>;
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
            setDataByte<0>(theBytes[0]);
            setDataByte<1>(theBytes[1]);
            setDataByte<2>(theBytes[2]);
            setDataByte<3>(theBytes[3]);
            auto end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            setDataByte<0>(theBytes[4]);
            setDataByte<1>(theBytes[5]);
            setDataByte<2>(theBytes[6]);
            setDataByte<3>(theBytes[7]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            setDataByte<0>(theBytes[8]);
            setDataByte<1>(theBytes[9]);
            setDataByte<2>(theBytes[10]);
            setDataByte<3>(theBytes[11]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            singleCycleDelay();
            singleCycleDelay();
            setDataByte<0>(theBytes[12]);
            setDataByte<1>(theBytes[13]);
            setDataByte<2>(theBytes[14]);
            setDataByte<3>(theBytes[15]);
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
#define X(base) \
    if constexpr (isReadOperation) { \
        setDataByte<0>(theBytes[(base + 0)]); \
        setDataByte<1>(theBytes[(base + 1)]); \
        setDataByte<2>(theBytes[(base + 2)]); \
        setDataByte<3>(theBytes[(base + 3)]); \
    } else { \
        if (digitalRead<Pin::BE0>() == LOW) { \
            theBytes[(base + 0)] = getDataByte<0>(); \
        } \
        if (digitalRead<Pin::BE1>() == LOW) { \
            theBytes[(base + 1)] = getDataByte<1>(); \
        } \
        if (digitalRead<Pin::BE2>() == LOW) { \
            theBytes[(base + 2)] = getDataByte<2>(); \
        } \
        if (digitalRead<Pin::BE3>() == LOW) { \
            theBytes[(base + 3)] = getDataByte<3>(); \
        }  \
    }
    X(0);
    auto end = Platform::isBurstLast();
    signalReady();
    if (end) {
        return;
    }
    singleCycleDelay();
    singleCycleDelay();
    X(4);
    end = Platform::isBurstLast();
    signalReady();
    if (end) {
        return;
    }
    singleCycleDelay();
    singleCycleDelay();
    X(8);
    end = Platform::isBurstLast();
    signalReady();
    if (end) {
        return;
    }
    singleCycleDelay();
    singleCycleDelay();
    X(12);
    // don't sample blast at the end of the transaction
    signalReady();
#undef X

}
    [[gnu::always_inline]]
    inline
    static void
    doCommunication(volatile SplitWord128& body, uint8_t lowest) noexcept {
        doCommunication(&body.bytes[getWordByteOffset(lowest)], lowest);
    }
};

template<bool isReadOperation>
struct CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen> {
    using Self = CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen>;
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
                setDataByte<0>(theBytes[0]);
                setDataByte<1>(theBytes[1]);
                auto end = Platform::isBurstLast();
                signalReady();
                if (end) {
                    return;
                }
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            setDataByte<2>(theBytes[2]);
            setDataByte<3>(theBytes[3]);
            auto end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            setDataByte<0>(theBytes[4]);
            setDataByte<1>(theBytes[5]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            setDataByte<2>(theBytes[6]);
            setDataByte<3>(theBytes[7]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            setDataByte<0>(theBytes[8]);
            setDataByte<1>(theBytes[9]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            setDataByte<2>(theBytes[10]);
            setDataByte<3>(theBytes[11]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            setDataByte<0>(theBytes[12]);
            setDataByte<1>(theBytes[13]);
            end = Platform::isBurstLast();
            signalReady();
            if (end) {
                return;
            }
            // put in some amount of wait states before just signalling again
            // since we started on the lower half of a 32-bit word
            setDataByte<2>(theBytes[14]);
            setDataByte<3>(theBytes[15]);
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
                    setDataByte<d0>(theBytes[b0]); \
                    setDataByte<d1>(theBytes[b1]); \
                } else { \
                    if constexpr (later) { \
                        /* in this case, we will immediately terminate if the 
                         * upper byte enable bit is 1
                         *
                         * Also, since this is later on in the process, it
                         * should be safe to just propagate without performing
                         * the check itself
                         */ \
                        theBytes[b0] = getDataByte<d0>(); \
                        if (digitalRead<Pin:: BE ## d1 > () == LOW) { \
                            theBytes[b1] = getDataByte<d1>(); \
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
                            theBytes[b1] = getDataByte<d1>(); \
                            if (digitalRead<Pin:: BE ## d0 > () == LOW) { \
                                theBytes[b0] = getDataByte<d0>(); \
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
                            theBytes[d0] = getDataByte<d0>(); \
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




template<bool isReadOperation, NativeBusWidth width, TargetPeripheral p>
[[gnu::always_inline]]
inline
void sendOpcodeSize(uint8_t offset) noexcept {
    CommunicationKernel<isReadOperation, width>::template doFixedCommunication<OpcodeCount_v<p>>(offset);
}

template<bool isReadOperation, NativeBusWidth width>
[[gnu::always_inline]]
inline
void sendZero(uint8_t offset) noexcept {
    CommunicationKernel<isReadOperation, width>::template doFixedCommunication<0>(offset);
}

template<bool isReadOperation, NativeBusWidth width>
[[gnu::always_inline]]
inline
void sendBoolean(bool value, uint8_t offset) noexcept {
    if (value) {
        CommunicationKernel<isReadOperation, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
    } else {
        sendZero<isReadOperation, width>(offset);
    }
}


template<NativeBusWidth width>
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
                    sendBoolean<true, width>(true, offset);
                    return;
                case K::Size:
                    sendOpcodeSize<true, width, TargetPeripheral::Info>(offset);
                    return;
                case K::GetChipsetClock:
                    CommunicationKernel<true, width>::template doFixedCommunication<F_CPU>(offset);
                    return;
                case K::GetCPUClock:
                    CommunicationKernel<true, width>::template doFixedCommunication<F_CPU/2>(offset);
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
                    sendZero<true, width>(offset);
                    return;
            }
            return;
        case TargetPeripheral::Serial:
            switch (getFunctionCode<TargetPeripheral::Serial>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Serial>;
                case K::Available:
                    sendBoolean<true, width>(theSerial.isAvailable(), offset);
                    return;
                case K::Size:
                    sendOpcodeSize<true, width, TargetPeripheral::Serial>(offset);
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
                    sendZero<true, width>(offset);
                    return;
            }
            break;
        case TargetPeripheral::Timer:
            switch (getFunctionCode<TargetPeripheral::Timer>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Timer>;
                case K::Available:
                    sendBoolean<true, width>(timerInterface.isAvailable(), offset);
                    return;
                case K::Size:
                    sendOpcodeSize<true, width, TargetPeripheral::Timer>(offset);
                    return;
                case K::SystemTimerPrescalar:
                    body.bytes[0] = timerInterface.getSystemTimerPrescalar();
                    break;
                case K::SystemTimerComparisonValue:
                    body.bytes[0] = timerInterface.getSystemTimerComparisonValue();
                    break;
                default:
                    sendZero<true, width>(offset);
                    return;

            }
            break;
        case TargetPeripheral::Display:
            theDisplay.handleReadOperations(body, function, offset);
            break;
        default:
            break;
    }
    CommunicationKernel<true, width>::doCommunication(body, offset);
}
template<NativeBusWidth width>
[[gnu::always_inline]]
inline
void
performIOWriteGroup0(SplitWord128& body, uint8_t group, uint8_t function, uint8_t offset) noexcept {
    // unlike standard i960 operations, we only decode the data we actually care
    // about out of the packet when performing a write operation so at this
    // point it doesn't matter what kind of data we were actually given
    //
    // need to sample the address lines prior to grabbing data off the bus
    CommunicationKernel<false, width>::doCommunication(body, offset);
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
    return sectionMask + (offset & offsetMask);
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

inline 
void 
updateDataLinesDirection(uint8_t value) noexcept {
    if constexpr (EnablePortKCapture_D0_7) {
        DDRK = value;
    } else {
        dataLinesDirection_bytes[0] = value;
    }
    dataLinesDirection_bytes[1] = value;
    dataLinesDirection_bytes[2] = value;
    dataLinesDirection_bytes[3] = value;
}

template<NativeBusWidth width> 
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    SplitWord128 operation;
    uint8_t currentDirection = 0xFF;
    updateDataLinesDirection(currentDirection);
    DDRJ = 0;
    // disable pullups!
    Platform::setBank(0, typename TreatAsOnChipAccess::AccessMethod{});
    Platform::setBank(0, typename TreatAsOffChipAccess::AccessMethod{});
    while (true) {
        waitForDataState();
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
                    // the i960 directly controls the bank index of the IBUS
                    CommunicationKernel<false, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOnChipAccess::AccessMethod{}),
                            offset
                            );
                    break;


                case 0xF0:
                    performIOWriteGroup0<width>(operation, 
                            static_cast<uint8_t>(al >> 16),
                            static_cast<uint8_t>(al >> 8),
                            offset);
                    break;
                case 0xF1: case 0xF2: case 0xF3: 
                case 0xF4: case 0xF5: case 0xF6: case 0xF7: 
                case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                    CommunicationKernel<false, width>::template doFixedCommunication<0>(offset);
                    break;
                default: 
                    Platform::setBank(al,
                            typename TreatAsOffChipAccess::AccessMethod{});
                    CommunicationKernel<false, width>::doCommunication(
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
                    // the i960 directly controls the bank index of the IBUS
                    CommunicationKernel<true, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOnChipAccess::AccessMethod{}),
                            offset
                            );
                    break;
                case 0xF0:
                    performIOReadGroup0<width>(operation, 
                            static_cast<uint8_t>(al >> 16),
                            static_cast<uint8_t>(al >> 8),
                            offset);
                    break;
                case 0xF1: case 0xF2: case 0xF3:
                case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                case 0xF8: case 0xF9: case 0xFA: case 0xFB:
                case 0xFC: case 0xFD: case 0xFE: case 0xFF:
                    CommunicationKernel<true, width>::template doFixedCommunication<0>(offset);
                    break;
                default:
                    Platform::setBank(al,
                            typename TreatAsOffChipAccess::AccessMethod{});
                    CommunicationKernel<true, width>::doCommunication(
                            getTransactionWindow(al, typename TreatAsOffChipAccess::AccessMethod{}),
                            offset
                            );
                    break;
            }
        }
        endTransaction();
        singleCycleDelay();
    }
}

template<uint32_t maxFileSize = 1024ul * 1024ul, auto BufferSize = 16384>
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
    // setup the IBUS bank
    getDirectionRegister<Port::IBUS_Bank>() = 0xFF;
    getOutputRegister<Port::IBUS_Bank>() = 0;
    if constexpr (EnablePortKCapture_D0_7) {
        getDirectionRegister<Port::D0_7>() = 0xff;
        getOutputRegister<Port::D0_7>() = 0x00;
    }
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
template<NativeBusWidth width>
void
discoveryDebugKindAndDispatch() {
    executionBody<width>();
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



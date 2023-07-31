/*
i960SxChipset_Type103
Copyright (c) 2022-2023, Joshua Scoggins
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
#ifdef DIMM_WIDE
#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

// allocate 1024 bytes total
[[gnu::always_inline]] inline bool isBurstLast() noexcept { 
    return digitalRead<Pin::BLAST>() == LOW; 
}
template<bool waitForReady = false>
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    toggle<Pin::READY>();
    if constexpr (waitForReady) {
        // wait four cycles after to make sure that the ready signal has been
        // propagated to the i960
        insertCustomNopCount<4>();
    }
}

template<Pin enablePin>
[[gnu::always_inline]]
inline void 
ioExpWrite16(uint8_t address, uint16_t value) noexcept {
    digitalWrite<enablePin, LOW>();
    SPI.transfer(0b0100'0000); // opcode
    SPI.transfer(address);
    SPI.transfer(static_cast<uint8_t>(value));
    SPI.transfer(static_cast<uint8_t>(value >> 8));
    digitalWrite<enablePin, HIGH>();
}

template<Pin enablePin>
[[gnu::always_inline]]
inline void 
ioExpWrite8(uint8_t address, uint8_t second) noexcept {
    digitalWrite<enablePin, LOW>();
    SPI.transfer(0b0100'0000); // opcode
    SPI.transfer(address);
    SPI.transfer(second);
    digitalWrite<enablePin, HIGH>();
}

template<Pin enablePin>
[[gnu::always_inline]]
inline uint16_t 
ioExpRead16(uint8_t address) noexcept {
    digitalWrite<enablePin, LOW>();
    SPI.transfer(0b0100'0001); // opcode
    SPI.transfer(address);
    uint16_t value = static_cast<uint16_t>(SPI.transfer(0));
    value |= (static_cast<uint16_t>(SPI.transfer(0)) << 8);
    digitalWrite<enablePin, HIGH>();
    return value;
}
template<Pin enablePin>
[[gnu::always_inline]]
inline uint8_t 
ioExpRead8(uint8_t address) noexcept {
    digitalWrite<enablePin, LOW>();
    SPI.transfer(0b0100'0001); // opcode
    SPI.transfer(address);
    uint8_t value = SPI.transfer(0);
    digitalWrite<enablePin, HIGH>();
    return value;
}
using Register8 = volatile uint8_t&;
using Register16 = volatile uint16_t&;
template<int index>
struct TimerDescriptor { 
};
#define X(index) \
template<> \
struct TimerDescriptor< index > {  \
    static inline Register8 TCCRxA = TCCR ## index ## A ; \
    static inline Register8 TCCRxB = TCCR ## index ## B ; \
    static inline Register8 TCCRxC = TCCR ## index ## C ; \
    static inline Register16 TCNTx = TCNT ## index; \
    static inline Register16 ICRx = ICR ## index ; \
    static inline Register16 OCRxA = OCR ## index ## A ; \
    static inline Register16 OCRxB = OCR ## index ## B ; \
    static inline Register16 OCRxC = OCR ## index ## C ; \
}; \
constexpr TimerDescriptor< index > timer ## index 
X(1);
X(3);
X(4);
X(5);
#undef X
void 
putCPUInReset() noexcept {
    
    /// @todo implement
}
void 
pullCPUOutOfReset() noexcept {
    /// @todo implement
}
template<bool isReadOperation>
struct RWOperation final {
    static constexpr bool IsReadOperation = isReadOperation;
};
using ReadOperation = RWOperation<true>;
using WriteOperation = RWOperation<false>;

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;
#if 0
[[gnu::address(0x2208)]] volatile uint8_t dataLines[4];
[[gnu::address(0x2208)]] volatile uint32_t dataLinesFull;
[[gnu::address(0x2208)]] volatile uint16_t dataLinesHalves[2];
[[gnu::address(0x220C)]] volatile uint32_t dataLinesDirection;
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_bytes[4];
[[gnu::address(0x220C)]] volatile uint8_t dataLinesDirection_LSB;

[[gnu::address(0x2200)]] volatile uint16_t AddressLines16Ptr[4];
[[gnu::address(0x2200)]] volatile uint32_t AddressLines32Ptr[2];
[[gnu::address(0x2200)]] volatile uint32_t addressLinesValue32;
[[gnu::address(0x2200)]] volatile uint16_t addressLinesLowerHalf;
[[gnu::address(0x2200)]] volatile uint8_t addressLines[8];
[[gnu::address(0x2200)]] volatile uint8_t addressLinesLowest;

template<NativeBusWidth width>
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}
template<uint16_t sectionMask, uint16_t offsetMask>
constexpr
uint16_t
computeTransactionWindow(uint16_t offset) noexcept {
    return sectionMask | (offset & offsetMask);
}

FORCE_INLINE
inline
DataRegister8
getTransactionWindow() noexcept {
    return memoryPointer<uint8_t>(computeTransactionWindow<0x4000, 0x3FFF>(addressLinesLowerHalf));
}
template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 4, "Invalid index provided to setDataByte, must be less than 4");
    if constexpr (index < 4) {
        dataLines[index] = value;
    }
}

FORCE_INLINE
inline void setDataByte(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
    setDataByte<0>(a);
    setDataByte<1>(b);
    setDataByte<2>(c);
    setDataByte<3>(d);
}

template<uint8_t index>
requires (index < 4)
inline uint8_t getDataByte() noexcept {
    static_assert(index < 4, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 4) {
        switch (index) {
            case 0:
            case 1:
            case 2:
            case 3:

        }
        return dataLines[index];
    } else {
        return 0;
    }
}
/**
 * @brief Just go through the motions of a write operation but do not capture
 * anything being sent by the i960
 */
FORCE_INLINE
inline void 
idleTransaction() noexcept {
    // just keep going until we are done
    while (true) {
        if (isBurstLast()) {
            signalReady<true>();
            return;
        }
        signalReady<true>();
    }
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

FORCE_INLINE
inline
static void
doCommunication() noexcept {
        auto theBytes = getTransactionWindow(); 
#define X(base) \
    if constexpr (isReadOperation) { \
        auto a = theBytes[(base + 0)]; \
        auto b = theBytes[(base + 1)]; \
        auto c = theBytes[(base + 2)]; \
        auto d = theBytes[(base + 3)]; \
        setDataByte(a, b, c, d); \
    } else { \
        auto a = getDataByte<0>(); \
        auto b = getDataByte<1>(); \
        auto c = getDataByte<2>(); \
        auto d = getDataByte<3>(); \
        if (digitalRead<Pin::BE0>() == LOW) { \
            theBytes[(base + 0)] = a; \
        } \
        if (digitalRead<Pin::BE1>() == LOW) { \
            theBytes[(base + 1)] = b; \
        } \
        if (digitalRead<Pin::BE2>() == LOW) { \
            theBytes[(base + 2)] = c; \
        } \
        if (digitalRead<Pin::BE3>() == LOW) { \
            theBytes[(base + 3)] = d; \
        } \
    }
        X(0);
        if (isBurstLast()) {
            goto Done;
        }
        signalReady<!isReadOperation>();
        X(4);
        if (isBurstLast()) {
            goto Done;
        }
        signalReady<!isReadOperation>();
        X(8);
        if (isBurstLast()) {
            goto Done;
        }
        signalReady<!isReadOperation>();
        X(12);
Done:
        signalReady<true>();
#undef X

}
FORCE_INLINE 
inline 
static void doIO() noexcept { 
        switch (addressLines[0]) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            dataLinesFull = F_CPU;
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            dataLinesFull = F_CPU / 2;
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = Serial.read();
                            dataLinesHalves[1] = 0;
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(dataLinesHalves[0]));
                        } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 12: { 
                         Serial.flush();
                         if constexpr (isReadOperation) { 
                             dataLinesFull = 0;
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
                     break;
#define X(obj, index) \
            case index + 0: { \
                        /* TCCRnA and TCCRnB */ \
                        if constexpr (isReadOperation) { \
                            setDataByte(obj.TCCRxA, obj.TCCRxB, obj.TCCRxC, 0);\
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                obj.TCCRxA = getDataByte<0>();\
                            } \
                            if (digitalRead<Pin::BE1>() == LOW) { \
                                obj.TCCRxB = getDataByte<1>();\
                            } \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                obj.TCCRxC = getDataByte<2>();\
                            } \
                        } \
                        if (isBurstLast()) {\
                            break; \
                        }\
                        signalReady<true>();  \
                    } \
            case index + 4: { \
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.TCNTx;\
                            auto tmp2 = obj.ICRx;\
                            interrupts(); \
                            dataLinesHalves[0] = tmp; \
                            dataLinesHalves[1] = tmp2;\
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW && \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
                                noInterrupts(); \
                                obj.TCNTx = value;\
                                interrupts(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.ICRx = value;\
                                interrupts(); \
                            } \
                        } \
                        if (isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case index + 8: { \
                        /* OCRnA should only be accessible if you do a full 16-bit write */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.OCRxA;\
                             auto tmp2 = obj.OCRxB;\
                            interrupts(); \
                            dataLinesHalves[0] = tmp; \
                            dataLinesHalves[1] = tmp2; \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
                                noInterrupts(); \
                                obj.OCRxA = value;\
                                interrupts(); \
                            } \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.OCRxB = value; \
                                interrupts(); \
                             } \
                        } \
                        if (isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case index + 12: { \
                         /* OCRnC */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             auto tmp = obj.OCRxC; \
                             interrupts(); \
                             dataLinesHalves[0] = tmp; \
                             dataLinesHalves[1] = 0;\
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = dataLinesHalves[0]; \
                                  noInterrupts(); \
                                  obj.OCRxC = value;\
                                  interrupts(); \
                              }\
                         } \
                         if (isBurstLast()) {\
                             break;\
                         } \
                         signalReady<true>(); \
                         break;\
                     } 
#ifdef TCCR1A
                    X(timer1, 0x10);
#endif
#ifdef TCCR3A
                    X(timer3, 0x20);
#endif
#ifdef TCCR4A
                    X(timer4, 0x30);
#endif
#ifdef TCCR5A
                    X(timer5, 0x40);
#endif
#undef X
            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
}
FORCE_INLINE
inline
static
void
dispatch() noexcept {
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        doCommunication();
    } else {
        // io operation
        doIO();
    }
}
};

template<bool isReadOperation>
struct CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen> {
    using Self = CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen>;
    static constexpr auto BusWidth = NativeBusWidth::Sixteen;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

public:
    //[[gnu::optimize("no-reorder-blocks")]]
    FORCE_INLINE
    inline
    static void
    doCommunication() noexcept {
        auto theBytes = getTransactionWindow(); 
        // figure out which word we are currently looking at
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
        //
        // since we are using the pointer directly we have to be a little more
        // creative. The base offsets have been modified
        if ((reinterpret_cast<uintptr_t>(theBytes) & 0b10) == 0) [[gnu::likely]] {
            if constexpr (isReadOperation) {
                dataLinesFull = reinterpret_cast<DataRegister32>(theBytes)[0];
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                dataLinesFull = reinterpret_cast<DataRegister32>(theBytes)[1];
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                dataLinesFull = reinterpret_cast<DataRegister32>(theBytes)[2];
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                dataLinesFull = reinterpret_cast<DataRegister32>(theBytes)[3];
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
            } else {
                {
                    auto lowest = dataLines[0];
                    auto lower = dataLines[1];
                    if (digitalRead<Pin::BE0>() == LOW) {
                        theBytes[0] = lowest;
                    }
                    if (digitalRead<Pin::BE1>() == LOW) {
                        theBytes[1] = lower;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[2];
                    auto highest = dataLines[3];
                    theBytes[2] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE3>() == LOW) {
                            theBytes[3] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[3] = highest;
                        signalReady<true>();
                    }
                }
                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[0];
                    auto lower = dataLines[1];
                    if (isBurstLast()) {
                        theBytes[4] = lowest;
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[5] = lower;
                        }
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[2];
                    auto highest = dataLines[3];
                    theBytes[4] = lowest;
                    theBytes[5] = lower;
                    theBytes[6] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE3>() == LOW) {
                            theBytes[7] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[7] = highest;
                        signalReady<true>();
                    }
                }
                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[0];
                    auto lower = dataLines[1];
                    if (isBurstLast()) {
                        theBytes[8] = lowest;
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[9] = lower;
                        }
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[2];
                    auto highest = dataLines[3];
                    theBytes[8] = lowest;
                    theBytes[9] = lower;
                    theBytes[10] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE3>() == LOW) {
                            theBytes[11] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[11] = highest;
                        signalReady<true>();
                    }
                }

                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[0];
                    auto lower = dataLines[1];
                    if (isBurstLast()) {
                        theBytes[12] = lowest;
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[13] = lower;
                        }
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[2];
                    auto highest = dataLines[3];
                    theBytes[12] = lowest;
                    theBytes[13] = lower;
                    theBytes[14] = higher;
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[15] = highest;
                    }
                    goto Done;
                }
            }
        } else {
            // because we are operating on 16-byte windows, there is no way
            // that we would ever fully access the entire 16-bytes in a single
            // transaction if we started unaligned like this
            if constexpr (isReadOperation) {
                setDataByte(theBytes[2], theBytes[3], theBytes[0], theBytes[1]);
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                setDataByte(theBytes[6], theBytes[7], theBytes[4], theBytes[5]);
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                setDataByte(theBytes[10], theBytes[11], theBytes[8], theBytes[9]);
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<false>();
                setDataByte(theBytes[14], theBytes[15], theBytes[12], theBytes[13]);
                if (isBurstLast()) {
                    goto Done; 
                }
                signalReady<true>();
            } else {
                {
                    auto lowest = dataLines[2];
                    auto lower = dataLines[3];
                    if (digitalRead<Pin::BE2>() == LOW) {
                        theBytes[0] = lowest;
                    }
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[1] = lower;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[0];
                    auto highest = dataLines[1];
                    theBytes[2] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[3] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[3] = highest;
                        signalReady<true>();
                    }
                }
                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[2];
                    auto lower = dataLines[3];
                    if (isBurstLast()) {
                        theBytes[4] = lowest;
                        if (digitalRead<Pin::BE3>() == LOW) {
                            theBytes[5] = lower;
                        }
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[0];
                    auto highest = dataLines[1];
                    theBytes[4] = lowest;
                    theBytes[5] = lower;
                    theBytes[6] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[7] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[7] = highest;
                        signalReady<true>();
                    }
                }
                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[2];
                    auto lower = dataLines[3];
                    if (isBurstLast()) {
                        theBytes[8] = lowest;
                        if (digitalRead<Pin::BE3>() == LOW) {
                            theBytes[9] = lower;
                        }
                        goto Done;
                    }
                    signalReady<true>();
                    auto higher = dataLines[0];
                    auto highest = dataLines[1];
                    theBytes[8] = lowest;
                    theBytes[9] = lower;
                    theBytes[10] = higher;
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        if (digitalRead<Pin::BE1>() == LOW) {
                            theBytes[11] = highest;
                        }
                        goto Done;
                    } else {
                        // we know that all of these entries must be valid so
                        // don't check the values
                        theBytes[11] = highest;
                        signalReady<true>();
                    }
                }

                {
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto lowest = dataLines[2];
                    auto lower = dataLines[3];
                    theBytes[12] = lowest;
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[13] = lower;
                    }
                }
            }
        }
Done:
        signalReady<true>();
    }
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady<true>()

FORCE_INLINE 
inline 
static void doIO() noexcept { 
        switch (addressLines[0]) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU);
                        } 
                        I960_Signal_Switch;
                    } 
            case 2: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU) >> 16);
                        } 
                        I960_Signal_Switch;
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU / 2);
                        } 
                        I960_Signal_Switch;
                    } 
            case 6: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU / 2) >> 16);
                        } 
                        I960_Signal_Switch;
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = Serial.read();
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(getDataByte<0>()));
                        } 
                        I960_Signal_Switch;
                    } 
            case 10: {
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[1] = 0;
                         } 
                        I960_Signal_Switch;
                     } 
            case 12: { 
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[0] = 0; 
                         } else { 
                             Serial.flush();
                         }
                         I960_Signal_Switch;
                     }
            case 14: {
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                     }
                     break;
#define X(obj, offset) \
            case offset + 0: { \
                        /* TCCRnA and TCCRnB */ \
                        if constexpr (isReadOperation) { \
                            setDataByte<0>(obj.TCCRxA);\
                            setDataByte<1>(obj.TCCRxB);\
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                obj.TCCRxA = getDataByte<0>();\
                            } \
                            if (digitalRead<Pin::BE1>() == LOW) { \
                                obj.TCCRxB = getDataByte<1>();\
                            } \
                        } \
                        I960_Signal_Switch;\
                    } \
            case offset + 2: { \
                        /* TCCRnC and Reserved (ignore that) */ \
                        if constexpr (isReadOperation) { \
                            setDataByte<2>(obj.TCCRxC);\
                            setDataByte<3>(0); \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                obj.TCCRxC = getDataByte<2>();\
                            } \
                        } \
                        I960_Signal_Switch;\
                    } \
            case offset + 4: { \
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.TCNTx; \
                            interrupts();  \
                            dataLinesHalves[0] = tmp;  \
                        } else {  \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) {  \
                                auto value = dataLinesHalves[0];  \
                                noInterrupts();  \
                                obj.TCNTx = value; \
                                interrupts();  \
                            }  \
                        }  \
                        I960_Signal_Switch; \
                    }  \
            case offset + 6: {  \
                        /* ICRn should only be accessible if you do a full 16-bit 
                         * write
                         */  \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.ICRx;\
                            interrupts(); \
                            dataLinesHalves[1] = tmp; \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.ICRx = value;\
                                interrupts(); \
                            } \
                        } \
                        I960_Signal_Switch;\
                    } \
            case offset + 8: { \
                        /* OCRnA should only be accessible if you do a full 16-bit write */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            auto tmp = obj.OCRxA;\
                            interrupts(); \
                            dataLinesHalves[0] = tmp; \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
                                noInterrupts(); \
                                obj.OCRxA = value;\
                                interrupts(); \
                            } \
                        } \
                        I960_Signal_Switch;\
                    } \
            case offset + 10: {\
                         /* OCRnB */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             auto tmp = obj.OCRxB;\
                             interrupts(); \
                             dataLinesHalves[1] = tmp; \
                         } else { \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = dataLinesHalves[1]; \
                                noInterrupts(); \
                                obj.OCRxB = value; \
                                interrupts(); \
                             } \
                         } \
                        I960_Signal_Switch;\
                     } \
            case offset + 12: { \
                         /* OCRnC */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             auto tmp = obj.OCRxC; \
                             interrupts(); \
                             dataLinesHalves[0] = tmp; \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = dataLinesHalves[0]; \
                                  noInterrupts(); \
                                  obj.OCRxC = value;\
                                  interrupts(); \
                              }\
                         } \
                        I960_Signal_Switch;\
                     } \
            case offset + 14: { \
                        /* nothing to do on writes but do update the data port
                         * on reads */ \
                         if constexpr (isReadOperation) { \
                            dataLinesHalves[1] = 0; \
                         } \
                         break;\
                     }  
#ifdef TCCR1A
                              X(timer1, 0x10);
#endif
#ifdef TCCR3A
                              X(timer3, 0x20);
#endif
#ifdef TCCR4A
                              X(timer4, 0x30);
#endif
#ifdef TCCR5A
                              X(timer5, 0x40);
#endif
#undef X


            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
}
#undef I960_Signal_Switch
FORCE_INLINE
inline
static
void
dispatch() noexcept {
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        doCommunication();
    } else {
        // io operation
        doIO();
    }
}
};



#endif
template<uint8_t value>
[[gnu::always_inline]]
inline 
void 
updateDataLinesDirection() noexcept {
    getDirectionRegister<Port::D0_7>() = value;
    getDirectionRegister<Port::D8_15>() = value;
    getDirectionRegister<Port::D16_23>() = value;
    getDirectionRegister<Port::D24_31>() = value;
}

template<NativeBusWidth width> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    // turn off the timer0 interrupt for system count, we don't care about it
    // anymore
    //digitalWrite<Pin::DirectionOutput, HIGH>();
    //getOutputRegister<Port::IBUS_Bank>() = 0;
    //getDirectionRegister<Port::IBUS_Bank>() = 0;
    // switch the XBUS bank mode to i960 instead of AVR
    // I want to use the upper four bits the XBUS address lines
    // while I can directly connect to the address lines, I want to test to
    // make sure that this works as well
    //ControlSignals.ctl.bankSelect = 1;

    //XBUSBankRegister.view32.data = 0;
    // at this point, we are setup to be in output mode (or read) and that is the
    // expected state for _all_ i960 processors, it will load some amount of
    // data from main memory to start the execution process. 
    //
    // After this point, we will never need to actually keep track of the
    // contents of the DirectionOutput pin. We will always be properly
    // synchronized overall!
    //
    // It is not lost on me that this is goto nightmare bingo, however in this
    // case I need the extra control of the goto statement. Allowing the
    // compiler to try and do this instead leads to implicit jumping issues
    // where the compiler has way too much fun with its hands. It will over
    // optimize things and create problems!
#if 0
ReadOperationStart:
    // wait until DEN goes low
    while (digitalRead<Pin::DEN>());
    // check to see if we need to change directions
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change direction to input since we are doing read -> write
        updateDataLinesDirection<0>();
        // update the direction pin 
        //toggle<Pin::DirectionOutput>();
        // then jump into the write loop
        goto WriteOperationBypass;
    }
ReadOperationBypass:
    // standard read operation so do the normal dispatch
    CommunicationKernel<true, width>::dispatch();
    // start the read operation again
    goto ReadOperationStart;

WriteOperationStart:
    // wait until DEN goes low
    while (digitalRead<Pin::DEN>());
    // check to see if we need to change directions
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change data lines to be output since we are doing write -> read
        updateDataLinesDirection<0xFF>();
        // update the direction pin
        toggle<Pin::DirectionOutput>();
        // jump to the read loop
        goto ReadOperationBypass;
    } 
WriteOperationBypass:
    // standard write operation so do the normal dispatch for write operations
    CommunicationKernel<false, width>::dispatch();
    // restart the write loop
    goto WriteOperationStart;
    // we should never get here!
#endif
    while (true) {

    }
}

void 
setupPins() noexcept {
    // power down the ADC, TWI, and USART3
    // currently we can't use them

    // setup the direction of the data and address ports to be inputs to start
    getDirectionRegister<Port::A2_9>() = 0;
    updateDataLinesDirection<0>();
    // enable interrupt pin output
    pinMode<Pin::INT0_960_>(OUTPUT);
    pinMode<Pin::XINT2_960_>(OUTPUT);
    pinMode<Pin::XINT4_960_>(OUTPUT);
    pinMode<Pin::XINT6_960_>(OUTPUT);
    pinMode<Pin::BE0>(INPUT);
    pinMode<Pin::BE1>(INPUT);
    pinMode<Pin::BE2>(INPUT);
    pinMode<Pin::BE3>(INPUT);
    pinMode<Pin::DEN>(INPUT);
    pinMode<Pin::BLAST>(INPUT);
    pinMode<Pin::WR>(INPUT);
    pinMode<Pin::READY>(OUTPUT);
    pinMode<Pin::SD_EN>(OUTPUT);
    pinMode<Pin::IO_OPERATION>(INPUT);
    pinMode<Pin::IO_EXP_ENABLE>(OUTPUT);
    pinMode<Pin::DataDirection>(OUTPUT);
    digitalWrite<Pin::INT0_960_, HIGH>();
    digitalWrite<Pin::XINT2_960_, HIGH>();
    digitalWrite<Pin::XINT4_960_, HIGH>();
    digitalWrite<Pin::XINT6_960_, HIGH>();
    digitalWrite<Pin::READY, HIGH>();
    digitalWrite<Pin::SD_EN, HIGH>();
    digitalWrite<Pin::IO_EXP_ENABLE, HIGH>();
    digitalWrite<Pin::DataDirection, HIGH>();
    uint16_t iodir = 0b0000'0000'1101'0000;
    ioExpWrite16<Pin::IO_EXP_ENABLE>(0x00, iodir);
    ioExpWrite16<Pin::IO_EXP_ENABLE>(0x12, 0xFFFF);
}

CPUKind 
getInstalledCPUKind() noexcept { 
    return static_cast<CPUKind>(ioExpRead8<Pin::IO_EXP_ENABLE>(0x13) & 0b111);
}

void
setup() {
    SPI.begin();
    setupPins();
    putCPUInReset();
    Serial.begin(115200);
    // setup the IO Expanders
    switch (getInstalledCPUKind()) {
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
            Serial.println(F("Unknown i960 CPU detected!"));
            break;
    }
    // find firmware.bin and install it into the 512k block of memory
    pullCPUOutOfReset();
}
void 
loop() {
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            Serial.println(F("16-bit bus width detected"));
            executionBody<NativeBusWidth::Sixteen>();
            break;
        case NativeBusWidth::ThirtyTwo:
            Serial.println(F("32-bit bus width detected"));
            executionBody<NativeBusWidth::ThirtyTwo>();
            break;
        default:
            Serial.println(F("Undefined bus width detected (fallback to 32-bit)"));
            executionBody<NativeBusWidth::Unknown>();
            break;
    }
}
#endif

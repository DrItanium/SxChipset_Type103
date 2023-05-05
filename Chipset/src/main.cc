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
#include <SdFat.h>

#include "Types.h"
#include "Pinout.h"
#include "IOOpcodes.h"
#include "Peripheral.h"
#include "Setup.h"
#include "TimerDevice.h"
SdFs SD;
TimerDevice timerInterface;
// allocate 1024 bytes total

template<bool waitForReady = false>
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    Platform::signalReady();
    if constexpr (waitForReady) {
        // wait four cycles after to make sure that the ready signal has been
        // propagated to the i960
        insertCustomNopCount<4>();
    }
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
template<NativeBusWidth width>
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}

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

template<uint8_t index>
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 4, "Invalid index provided to setDataByte, must be less than 4");
    if constexpr (index < 4) {
        dataLines[index] = value;
    }
}
template<uint8_t index>
inline uint8_t getDataByte() noexcept {
    static_assert(index < 4, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 4) {
        return dataLines[index];
    } else {
        return 0;
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
        auto end = Platform::isBurstLast();
        signalReady<true>();
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
            setDataByte<0>(static_cast<uint8_t>(a));
            setDataByte<1>(static_cast<uint8_t>(a >> 8));
            setDataByte<2>(static_cast<uint8_t>(a >> 16));
            setDataByte<3>(static_cast<uint8_t>(a >> 24));
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
            const uint8_t* theBytes = &contents.bytes[getWordByteOffset<width>(lowest)]; 
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
doCommunication(DataRegister8 theBytes, uint8_t) noexcept {
#define X(base) \
    if constexpr (isReadOperation) { \
        auto a = theBytes[(base + 0)]; \
        auto b = theBytes[(base + 1)]; \
        auto c = theBytes[(base + 2)]; \
        auto d = theBytes[(base + 3)]; \
        setDataByte<0>(a); \
        setDataByte<1>(b); \
        setDataByte<2>(c); \
        setDataByte<3>(d); \
    } else { \
        if (digitalRead<Pin::BE0>() == LOW) { \
            auto a = getDataByte<0>(); \
            theBytes[(base + 0)] = a; \
        } \
        if (digitalRead<Pin::BE1>() == LOW) { \
            auto a = getDataByte<1>(); \
            theBytes[(base + 1)] = a; \
        } \
        if (digitalRead<Pin::BE2>() == LOW) { \
            auto a = getDataByte<2>(); \
            theBytes[(base + 2)] = a; \
        } \
        if (digitalRead<Pin::BE3>() == LOW) { \
            auto a = getDataByte<3>(); \
            theBytes[(base + 3)] = a; \
        } \
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
        doCommunication(&body.bytes[getWordByteOffset<width>(lowest)], lowest);
    }
#define X(index) \
static void doTimer ## index (uint8_t offset) noexcept { \
        switch (offset & 0b1110) { \
            case 0: { \
                        /* TCCRnA and TCCRnB */ \
                        /* TCCRnC and Reserved (ignore that) */ \
                        if constexpr (isReadOperation) { \
                            setDataByte<0>(TCCR ## index ## A); \
                            setDataByte<1>(TCCR ## index ## B); \
                            setDataByte<2>(TCCR ## index ## C); \
                            setDataByte<3>(0); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                TCCR ## index ## A = getDataByte<0>(); \
                            } \
                            if (digitalRead<Pin::BE1>() == LOW) { \
                                TCCR ## index ## B = getDataByte<1>(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                TCCR ## index ## C = getDataByte<2>(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>();  \
                    } \
            case 4: { \
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ \
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            dataLinesHalves[0] = TCNT ## index; \
                            dataLinesHalves[1] = ICR ## index; \
                            interrupts(); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW && \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                noInterrupts(); \
                                TCNT ## index  = dataLinesHalves[0]; \
                                interrupts(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                noInterrupts(); \
                                ICR ## index = dataLinesHalves[1]; \
                                interrupts(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case 8: { \
                        /* OCRnA should only be accessible if you do a full 16-bit write */ \
                        /* OCRnB */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            dataLinesHalves[0] = OCR ## index ## A ; \
                            dataLinesHalves[1] = OCR ## index ## B ; \
                            interrupts(); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                noInterrupts(); \
                                OCR ## index ## A = dataLinesHalves[0]; \
                                interrupts(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                noInterrupts(); \
                                OCR ## index ## B = dataLinesHalves[1]; \
                                interrupts(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                     } \
            case 12: { \
                         /* OCRnC */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             dataLinesHalves[0] = OCR ## index ## C ; \
                             interrupts(); \
                             dataLinesHalves[1] = 0; \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  noInterrupts(); \
                                  OCR ## index ## C = dataLinesHalves[0];\
                                  interrupts(); \
                              }\
                         } \
                     }  \
            default: break; \
        } \
        signalReady<true>(); \
}
X(1);
X(3);
X(4);
X(5);
#undef X
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
private:
    template<uint32_t a, uint32_t b = a, uint32_t c = a, uint32_t d = a>
    [[gnu::always_inline]]
    inline
    static void doFixedReadOperation(uint8_t lowest) noexcept {
        if constexpr (a == b && b == c && c == d) {
            setDataByte<0>(static_cast<uint8_t>(a));
            setDataByte<1>(static_cast<uint8_t>(a >> 8));
            setDataByte<2>(static_cast<uint8_t>(a >> 16));
            setDataByte<3>(static_cast<uint8_t>(a >> 24));
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
            uint8_t offset = getWordByteOffset<BusWidth>(value);
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
#define X(d0, b0, d1, b1, later) \
            { \
                static constexpr bool IsLastWord = (b0 == 14 && b1 == 15); \
                if constexpr (isReadOperation) { \
                    setDataByte<d0>(theBytes[b0]); \
                    setDataByte<d1>(theBytes[b1]); \
                    if (Platform::isBurstLast()) { \
                        break; \
                    } \
                    signalReady<!isReadOperation>(); \
                } else { \
                    if constexpr (later) { \
                        /* in this case, we will immediately terminate if the 
                         * upper byte enable bit is 1
                         *
                         * Also, since this is later on in the process, it
                         * should be safe to just propagate without performing
                         * the check itself
                         */ \
                        if constexpr (!IsLastWord) { \
                            if (digitalRead<Pin:: BE ## d1 >()) { \
                                theBytes[b0] = getDataByte<d0>(); \
                                break; \
                            } else { \
                                /* 
                                 * we could be operating on a 16-bit register
                                 * so follow the manual and write the upper
                                 * half first followed by the lower half every
                                 * time
                                 *
                                 */ \
                                theBytes[b1] = getDataByte<d1>(); \
                                theBytes[b0] = getDataByte<d0>(); \
                            } \
                            if (Platform::isBurstLast()) { \
                                break; \
                            } \
                            signalReady<!isReadOperation>(); \
                        } else { \
                            if (digitalRead<Pin:: BE ## d1 >()) { \
                                theBytes[b0] = getDataByte<d0>(); \
                                break; \
                            } \
                            theBytes[b1] = getDataByte<d1>(); \
                            theBytes[b0] = getDataByte<d0>(); \
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
                            if constexpr (!IsLastWord) { \
                                if (Platform::isBurstLast()) { \
                                    break; \
                                } \
                                signalReady<!isReadOperation>(); \
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
            }
#define LO(b0, b1, later) X(0, b0, 1, b1, later)
#define HI(b0, b1, later) X(2, b0, 3, b1, later)
private:
    template<bool isAligned>
    [[gnu::always_inline]]
    inline
    static void
    doCommunication_Internal(DataRegister8 theBytes) noexcept {
        do {
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
            if constexpr (isAligned) {
                LO(0, 1, false);
                HI(2, 3, true);
            } else {
                HI(2, 3, false);
            }
            LO(4, 5, true);
            HI(6, 7, true);
            LO(8, 9, true);
            HI(10, 11, true);
            LO(12, 13, true);
            HI(14, 15, true);
        } while (false);
        signalReady();
    }
#undef LO
#undef HI
#undef X
public:
    [[gnu::always_inline]]
    inline
    static void
    doCommunication(DataRegister8 theBytes, uint8_t offset) noexcept {
        if ((offset & 0b10) == 0) {
            doCommunication_Internal<true>(theBytes);
        } else {
            doCommunication_Internal<false>(theBytes);
        }
    }

    [[gnu::always_inline]]
    inline
    static void
    doCommunication(volatile SplitWord128& body, uint8_t lowest) noexcept {
        doCommunication(&body.bytes[getWordByteOffset<BusWidth>(lowest)], lowest);
    }
#define X(index) \
static void doTimer ## index (uint8_t offset) noexcept { \
        switch (offset & 0b1110) { \
            case 0: { \
                        /* TCCRnA and TCCRnB */ \
                        if constexpr (isReadOperation) { \
                            setDataByte<0>(TCCR ## index ## A); \
                            setDataByte<1>(TCCR ## index ## B); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                TCCR ## index ## A = getDataByte<0>(); \
                            } \
                            if (digitalRead<Pin::BE1>() == LOW) { \
                                TCCR ## index ## B = getDataByte<1>(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>();  \
                    } \
            case 2: { \
                        /* TCCRnC and Reserved (ignore that) */ \
                        if constexpr (isReadOperation) { \
                            setDataByte<2>(TCCR ## index ## C); \
                            setDataByte<3>(0); \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW) { \
                                TCCR ## index ## C = getDataByte<2>(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>();  \
                    } \
            case 4: { \
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            dataLinesHalves[0] = TCNT ## index; \
                            interrupts(); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW && \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                noInterrupts(); \
                                TCNT ## index  = dataLinesHalves[0]; \
                                interrupts(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case 6: { \
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            dataLinesHalves[1] = ICR ## index ; \
                            interrupts(); \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                noInterrupts(); \
                                ICR ## index = dataLinesHalves[1]; \
                                interrupts(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case 8: { \
                        /* OCRnA should only be accessible if you do a full 16-bit write */ \
                        if constexpr (isReadOperation) { \
                            noInterrupts(); \
                            dataLinesHalves[0] = OCR ## index ## A ; \
                            interrupts(); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                noInterrupts(); \
                                OCR ## index ## A = dataLinesHalves[0]; \
                                interrupts(); \
                            } \
                        } \
                        if (Platform::isBurstLast()) { \
                            break; \
                        } \
                        signalReady<true>(); \
                    } \
            case 10: { \
                         /* OCRnB */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             dataLinesHalves[1] = OCR ## index ## B ; \
                             interrupts(); \
                         } else { \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                noInterrupts(); \
                                OCR ## index ## B = dataLinesHalves[1]; \
                                interrupts(); \
                             } \
                         } \
                         if (Platform::isBurstLast()) { \
                             break; \
                         } \
                         signalReady<true>(); \
                     } \
            case 12: { \
                         /* OCRnC */ \
                         if constexpr (isReadOperation) { \
                             noInterrupts(); \
                             dataLinesHalves[0] = OCR ## index ## C ; \
                             interrupts(); \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  noInterrupts(); \
                                  OCR ## index ## C = dataLinesHalves[0];\
                                  interrupts(); \
                              }\
                         } \
                         if (Platform::isBurstLast()) {\
                             break;\
                         } \
                         signalReady<true>(); \
                     } \
            case 14: { \
                        /* nothing to do on writes but do update the data port
                         * on reads */ \
                         if constexpr (isReadOperation) { \
                            dataLinesHalves[1] = 0; \
                         } \
                     }  \
            default: break; \
        } \
        signalReady<true>(); \
}
X(1);
X(3);
X(4);
X(5);
#undef X
};

template<bool isReadOperation, NativeBusWidth width>
[[gnu::always_inline]]
inline
void sendZero(uint8_t offset) noexcept {
    CommunicationKernel<isReadOperation, width>::template doFixedCommunication<0>(offset);
}

volatile SplitWord128 operation;
template<NativeBusWidth width>
[[gnu::always_inline]]
inline
void
performIOReadGroup0(uint16_t opcode) noexcept {
    // unlike standard i960 operations, we only encode the data we actually care
    // about out of the packet when performing a read operation so at this
    // point it doesn't matter what kind of data the i960 is requesting.
    // This maintains consistency and makes the implementation much simpler
    using K = IOOpcodes;
    const uint8_t offset = static_cast<uint8_t>(opcode);
    switch (static_cast<IOOpcodes>(opcode)) {
        case K::Info_GetChipsetClockSpeed:
            CommunicationKernel<true, width>::template doFixedCommunication<F_CPU>(0);
            return;
        case K::Info_GetCPUClockSpeed:
            CommunicationKernel<true, width>::template doFixedCommunication<F_CPU/2>(0);
            return;
        case K::Serial_RW:
            operation[0].halves[0] = Serial.read();
            break;
#ifdef TCCR1A
        case K::Timer1:
            CommunicationKernel<true, width>::doTimer1(offset);
            return;
#endif
#ifdef TCCR3A
        case K::Timer3:
            CommunicationKernel<true, width>::doTimer3(offset);
            return;
#endif
#ifdef TCCR4A
        case K::Timer4:
            CommunicationKernel<true, width>::doTimer4(offset);
            return;
#endif
#ifdef TCCR5A
        case K::Timer5:
            CommunicationKernel<true, width>::doTimer5(offset);
            return;
#endif
        default:
            sendZero<true, width>(0);
            return;
    }
    CommunicationKernel<true, width>::doCommunication(operation, offset);
}
template<NativeBusWidth width>
[[gnu::always_inline]]
inline
void
performIOWriteGroup0(uint16_t opcode) noexcept {
    // unlike standard i960 operations, we only decode the data we actually care
    // about out of the packet when performing a write operation so at this
    // point it doesn't matter what kind of data we were actually given
    //
    // need to sample the address lines prior to grabbing data off the bus
    using K = IOOpcodes;
    const uint8_t offset = static_cast<uint8_t>(opcode);
    switch (static_cast<K>(opcode)) {
        case K::Serial_RW: {
                               CommunicationKernel<false, width>::doCommunication(operation, offset);
                               asm volatile ("nop");
                               Serial.write(static_cast<uint8_t>(operation.bytes[0]));
                               break;
                           }
        case K::Serial_Flush: {
                                  Serial.flush();
                                  idleTransaction();
                                  break;
                              }
#ifdef TCCR1A
        case K::Timer1:
            CommunicationKernel<false, width>::doTimer1(offset);
            break;
#endif
#ifdef TCCR3A
        case K::Timer3:
            CommunicationKernel<false, width>::doTimer3(offset);
            break;
#endif
#ifdef TCCR4A
        case K::Timer4:
            CommunicationKernel<false, width>::doTimer4(offset);
            break;
#endif
#ifdef TCCR5A
        case K::Timer5:
            CommunicationKernel<false, width>::doTimer5(offset);
            break;
#endif

        default: 
            idleTransaction();
            break;
    }
}

template<uint16_t sectionMask, uint16_t offsetMask>
constexpr
uint16_t
computeTransactionWindow_Generic(uint16_t offset) noexcept {
    return sectionMask | (offset & offsetMask);
}

template<NativeBusWidth width>
constexpr
uint16_t 
computeTransactionWindow(uint16_t offset, typename TreatAsOnChipAccess::AccessMethod) noexcept {
    return computeTransactionWindow_Generic<0x4000, width == NativeBusWidth::Sixteen ? 0x3FFC : 0x3FFF>(offset);
}

template<NativeBusWidth width, typename T>
DataRegister8
getTransactionWindow(uint16_t offset, T) noexcept {
    return memoryPointer<uint8_t>(computeTransactionWindow<width>(offset, T{}));
}

[[gnu::always_inline]]
inline 
void 
updateDataLinesDirection(uint8_t value) noexcept {
    dataLinesDirection_bytes[0] = value;
    dataLinesDirection_bytes[1] = value;
    dataLinesDirection_bytes[2] = value;
    dataLinesDirection_bytes[3] = value;
}
template<NativeBusWidth width> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    digitalWrite<Pin::DirectionOutput, HIGH>();
    getDirectionRegister<Port::IBUS_Bank>() = 0;
    // switch the XBUS bank mode to i960 instead of AVR
    // I want to use the upper four bits the XBUS address lines
    // while I can directly connect to the address lines, I want to test to
    // make sure that this works as well
    ControlSignals.ctl.bankSelect = 1;

    // disable pullups!
    Platform::setBank(0, typename TreatAsOnChipAccess::AccessMethod{});
    Platform::setBank(0, typename TreatAsOffChipAccess::AccessMethod{});
    while (true) {
        // only check currentDirection once at the start of the transaction
        if (sampleOutputState<Pin::DirectionOutput>()) {
            // start in read
            waitForDataState();
            startTransaction();
            const uint16_t al = addressLinesLowerHalf;
            // okay so we know that we are going to write so don't query the
            // pin!
            if (const auto offset = static_cast<uint8_t>(al); !digitalRead<Pin::ChangeDirection>()) {
                // read -> write
                updateDataLinesDirection(0);
                toggle<Pin::DirectionOutput>();
                if (digitalRead<Pin::IsMemorySpaceOperation>()) {
                    // the IBUS is the window into the 32-bit bus that the i960 is
                    // accessing from. Right now, it supports up to 4 megabytes of
                    // space (repeating these 4 megabytes throughout the full
                    // 32-bit space until we get to IO space)
                    auto window = getTransactionWindow<width>(al, typename TreatAsOnChipAccess::AccessMethod{}); 
                    // read -> write
                    CommunicationKernel<false, width>::doCommunication( window, offset);


                } else {
                    // read -> write
                    performIOWriteGroup0<width>(al);
                }
            } else {
                // read -> read
                // we are staying as a read operation!
                if (digitalRead<Pin::IsMemorySpaceOperation>()) {
                    // the IBUS is the window into the 32-bit bus that the i960 is
                    // accessing from. Right now, it supports up to 4 megabytes of
                    // space (repeating these 4 megabytes throughout the full
                    // 32-bit space until we get to IO space)
                    auto window = getTransactionWindow<width>(al, typename TreatAsOnChipAccess::AccessMethod{});
                    CommunicationKernel<true, width>::doCommunication( window, offset);
                } else {
                    // read -> read
                    performIOReadGroup0<width>(al);
                }
            }
            // since it is not zero we are looking at what was previously a read operation
            endTransaction();
        } else {
            // start in write
            waitForDataState();
            startTransaction();
            const uint16_t al = addressLinesLowerHalf;
            if (const auto offset = static_cast<uint8_t>(al); !digitalRead<Pin::ChangeDirection>()) {
                // write -> read
                updateDataLinesDirection(0xFF);
                toggle<Pin::DirectionOutput>();
                if (digitalRead<Pin::IsMemorySpaceOperation>()) {
                    auto window = getTransactionWindow<width>(al, typename TreatAsOnChipAccess::AccessMethod{}); 
                    // write -> read
                    // the IBUS is the window into the 32-bit bus that the i960 is
                    // accessing from. Right now, it supports up to 4 megabytes of
                    // space (repeating these 4 megabytes throughout the full
                    // 32-bit space until we get to IO space)
                    CommunicationKernel<true, width>::doCommunication( window, offset);
                } else {
                    // write -> read
                    performIOReadGroup0<width>(al);
                }
            } else {
                // write -> write
                if (digitalRead<Pin::IsMemorySpaceOperation>()) {
                    auto window = getTransactionWindow<width>(al, typename TreatAsOnChipAccess::AccessMethod{}); 
                    // write -> write
                    // the IBUS is the window into the 32-bit bus that the i960 is
                    // accessing from. Right now, it supports up to 4 megabytes of
                    // space (repeating these 4 megabytes throughout the full
                    // 32-bit space until we get to IO space)
                    CommunicationKernel<false, width>::doCommunication( window, offset);
                } else {
                    // write -> write
                    performIOWriteGroup0<width>(al);
                }
            }
            // currently a write operation
            endTransaction();
        }
        // put the single cycle delay back in to be on the safe side
        singleCycleDelay();
        singleCycleDelay();
    }
}

template<uint32_t maxFileSize = 1024ul * 1024ul, auto BufferSize = 16384>
[[gnu::noinline]]
void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = maxFileSize;
    Serial.println(F("Looking for an SD Card!"));
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
    // okay so now end reading from the SD Card
    SD.end();
}
void 
setupPins() noexcept {
    // power down the ADC, TWI, and USART3
    // currently we can't use them
    PRR0 = 0b1000'0001; // deactivate TWI and ADC
    PRR1 = 0b00000'100; // deactivate USART3

    // enable interrupt pin output
    pinMode<Pin::INT0_960_>(OUTPUT);
    digitalWrite<Pin::INT0_960_, HIGH>();
    // setup the IBUS bank
    getDirectionRegister<Port::IBUS_Bank>() = 0xFF;
    getOutputRegister<Port::IBUS_Bank>() = 0;
    pinMode(Pin::IsMemorySpaceOperation, INPUT);
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
    pinMode(Pin::DirectionOutput, OUTPUT);
    // we start with 0xFF for the direction output so reflect it here
    digitalWrite<Pin::DirectionOutput, HIGH>();
    pinMode(Pin::ChangeDirection, INPUT);
    if constexpr (MCUHasDirectAccess) {
        pinMode(Pin::READY, OUTPUT);
        digitalWrite<Pin::READY, HIGH>();
    }
}
void
setup() {
    setupPins();
    Serial.begin(115200);
    //timerInterface.begin();
    // setup the IO Expanders
    Platform::begin();
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
            Serial.println(F("Unknown i960 CPU detected!"));
            break;
    }
    // find firmware.bin and install it into the 512k block of memory
    SPI.begin();
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    installMemoryImage();
    SPI.endTransaction();
    SPI.end();
    pullCPUOutOfReset();
}
void 
loop() {
    switch (getBusWidth(Platform::getInstalledCPUKind())) {
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

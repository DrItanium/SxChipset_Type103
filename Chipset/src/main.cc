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

#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "IOOpcodes.h"
#include "Peripheral.h"
#include "Setup.h"
// allocate 1024 bytes total
constexpr bool EnableSpecialSpace = true;
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
    Platform::doReset(LOW);
}
void 
pullCPUOutOfReset() noexcept {
    Platform::doReset(HIGH);
}
void
waitForDataState() noexcept {
    while (digitalRead<Pin::DEN>() == HIGH); 
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

FORCE_INLINE
inline void setDataByte(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
    setDataByte<0>(a);
    setDataByte<1>(b);
    setDataByte<2>(c);
    setDataByte<3>(d);
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
FORCE_INLINE
inline void 
idleTransaction() noexcept {
    // just keep going until we are done
    do {
        if (isBurstLast()) {
            break;
        }
        signalReady<true>();
    } while (true);
    signalReady();
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
        auto theBytes = getTransactionWindow<width>(addressLinesLowerHalf, typename TreatAsOnChipAccess::AccessMethod{}); 
#define X(base) \
    if constexpr (isReadOperation) { \
        auto a = theBytes[(base + 0)]; \
        auto b = theBytes[(base + 1)]; \
        auto c = theBytes[(base + 2)]; \
        auto d = theBytes[(base + 3)]; \
        setDataByte(a, b, c, d); \
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
        do {
            X(0);
            if (isBurstLast()) {
                break;
            }
            signalReady<!isReadOperation>();
            X(4);
            if (isBurstLast()) {
                break;
            }
            signalReady<!isReadOperation>();
            X(8);
            if (isBurstLast()) {
                break;
            }
            signalReady<!isReadOperation>();
            X(12);
        } while (false);
        signalReady();
#undef X

}
FORCE_INLINE 
inline 
static void doIO() noexcept { 
        uint8_t offset = addressLines[0];
        switch (offset) { 
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
#ifdef TCCR1A
            case 0x10 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte(timer1.TCCRxA, timer1.TCCRxB, timer1.TCCRxC, 0);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer1.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer1.TCCRxB = getDataByte<1>();
                            } 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer1.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x10 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer1.TCNTx;
                            auto tmp2 = timer1.ICRx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2;
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer1.TCNTx = value;
                                interrupts(); 
                            } 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer1.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x10 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer1.OCRxA;
                             auto tmp2 = timer1.OCRxB;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer1.OCRxA = value;
                                interrupts(); 
                            } 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer1.OCRxB = value; 
                                interrupts(); 
                             } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x10 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer1.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                             dataLinesHalves[1] = 0;
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer1.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                         break;
                     } 
#endif
#ifdef TCCR3A
            case 0x20 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte(timer3.TCCRxA, timer3.TCCRxB, timer3.TCCRxC, 0);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer3.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer3.TCCRxB = getDataByte<1>();
                            } 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer3.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x20 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer3.TCNTx;
                            auto tmp2 = timer3.ICRx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2;
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer3.TCNTx = value;
                                interrupts(); 
                            } 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer3.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x20 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer3.OCRxA;
                             auto tmp2 = timer3.OCRxB;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer3.OCRxA = value;
                                interrupts(); 
                            } 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer3.OCRxB = value; 
                                interrupts(); 
                             } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x20 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer3.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                             dataLinesHalves[1] = 0;
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer3.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                         break;
                     } 
#endif
#ifdef TCCR4A
            case 0x30 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte(timer4.TCCRxA, timer4.TCCRxB, timer4.TCCRxC, 0);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer4.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer4.TCCRxB = getDataByte<1>();
                            } 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer4.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x30 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer4.TCNTx;
                            auto tmp2 = timer4.ICRx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2;
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer4.TCNTx = value;
                                interrupts(); 
                            } 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer4.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x30 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer4.OCRxA;
                             auto tmp2 = timer4.OCRxB;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer4.OCRxA = value;
                                interrupts(); 
                            } 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer4.OCRxB = value; 
                                interrupts(); 
                             } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x30 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer4.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                             dataLinesHalves[1] = 0;
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer4.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                         break;
                     } 
#endif
#ifdef TCCR5A
            case 0x40 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte(timer5.TCCRxA, timer5.TCCRxB, timer5.TCCRxC, 0);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer5.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer5.TCCRxB = getDataByte<1>();
                            } 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer5.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x40 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer5.TCNTx;
                            auto tmp2 = timer5.ICRx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2;
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer5.TCNTx = value;
                                interrupts(); 
                            } 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer5.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x40 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer5.OCRxA;
                             auto tmp2 = timer5.OCRxB;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                            dataLinesHalves[1] = tmp2; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer5.OCRxA = value;
                                interrupts(); 
                            } 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer5.OCRxB = value; 
                                interrupts(); 
                             } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x40 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer5.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                             dataLinesHalves[1] = 0;
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer5.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                         break;
                     } 
#endif
            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
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

private:
    template<uint8_t d0, uint8_t b0, uint8_t d1, uint8_t b1, bool later, Pin beLower, Pin beUpper>
    FORCE_INLINE
    inline
    static 
    void performCommunicationSingle(DataRegister8 theBytes) noexcept {
        static_assert(d0 == 0 || d0 == 2, "d0 must be 0 or 2");
        static_assert(d1 == 1 || d1 == 3, "d1 must be 1 or 3");
        static_assert(beLower == Pin::BE0 || beLower == Pin::BE2, "beLower must be BE0 or BE2");
        static_assert(beUpper == Pin::BE1 || beUpper == Pin::BE3, "beUpper must be BE1 or BE3");
        if constexpr (isReadOperation) { 
            auto lower = theBytes[b0]; 
            auto upper = theBytes[b1]; 
            setDataByte<d0>(lower); 
            setDataByte<d1>(upper); 
        } else { 
            /* in the case where later is true, we 
             * will not check the lower byte enable bit for the given
             * pair
             *
             * Also, since this is later on in the process, it
             * should be safe to just propagate without performing
             * the check itself
             *
             * However, the first time through, we want to make sure we
             * check both upper and lower.
             */ 
            if (later || digitalRead<beLower>() == LOW) { 
                theBytes[b0] = getDataByte<d0>(); 
            } 
            if (digitalRead<beUpper>() == LOW) { 
                theBytes[b1] = getDataByte<d1>(); 
            } 
        } 
    }
#define X(d0, b0, d1, b1, later) \
            { \
                static constexpr bool IsLastWord = (b0 == 14 && b1 == 15); \
                performCommunicationSingle<d0, b0, d1, b1, later, Pin :: BE ## d0 , Pin :: BE ## d1 >(theBytes); \
                if constexpr (!IsLastWord) { \
                    if (isBurstLast()) { \
                        break; \
                    } \
                    signalReady<!isReadOperation>(); \
                } \
            }
#define LO(b0, b1, later) X(0, b0, 1, b1, later)
#define HI(b0, b1, later) X(2, b0, 3, b1, later)
public:
    FORCE_INLINE
    inline
    static void
    doCommunication() noexcept {
            const uint16_t al = addressLinesLowerHalf;
            auto lowest = static_cast<uint8_t>(al);
            auto theBytes = getTransactionWindow<BusWidth>(al, typename TreatAsOnChipAccess::AccessMethod{}); 
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
                if ((lowest & 0b10) == 0) {
                    LO(0, 1, false);
                }
                HI(2, 3, false);
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

FORCE_INLINE 
inline 
static void doIO() noexcept { 
        uint8_t offset = addressLines[0];
        switch (offset) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU);
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 2: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU) >> 16);
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[0] = static_cast<uint16_t>(F_CPU / 2);
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 6: { 
                        if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = static_cast<uint16_t>((F_CPU / 2) >> 16);
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
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(getDataByte<0>()));
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 10: {
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[1] = 0;
                         } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             dataLinesHalves[0] = 0; 
                         } else { 
                             Serial.flush();
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
            case 14: { 
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                     }
                     break;
#ifdef TCCR1A
            case 0x10 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<0>(timer1.TCCRxA);
                            setDataByte<1>(timer1.TCCRxB);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer1.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer1.TCCRxB = getDataByte<1>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x10 + 2: { 
                        /* TCCRnC and Reserved (ignore that) */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<2>(timer1.TCCRxC);
                            setDataByte<3>(0); 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer1.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 0x10 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer1.TCNTx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer1.TCNTx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x10 + 6: { 
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer1.ICRx;
                            interrupts(); 
                            dataLinesHalves[1] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer1.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x10 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer1.OCRxA;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer1.OCRxA = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x10 + 10: {
                         /* OCRnB */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer1.OCRxB;
                             interrupts(); 
                             dataLinesHalves[1] = tmp; 
                         } else { 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer1.OCRxB = value; 
                                interrupts(); 
                             } 
                         } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 0x10 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer1.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer1.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
            case 0x10 + 14: { 
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                         break;
                     }  
#endif
#ifdef TCCR3A
            case 0x20 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<0>(timer3.TCCRxA);
                            setDataByte<1>(timer3.TCCRxB);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer3.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer3.TCCRxB = getDataByte<1>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x20 + 2: { 
                        /* TCCRnC and Reserved (ignore that) */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<2>(timer3.TCCRxC);
                            setDataByte<3>(0); 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer3.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 0x20 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer3.TCNTx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer3.TCNTx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x20 + 6: { 
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer3.ICRx;
                            interrupts(); 
                            dataLinesHalves[1] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer3.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x20 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer3.OCRxA;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer3.OCRxA = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x20 + 10: {
                         /* OCRnB */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer3.OCRxB;
                             interrupts(); 
                             dataLinesHalves[1] = tmp; 
                         } else { 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer3.OCRxB = value; 
                                interrupts(); 
                             } 
                         } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 0x20 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer3.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer3.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
            case 0x20 + 14: { 
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                         break;
                     }  
#endif
#ifdef TCCR4A
            case 0x30 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<0>(timer4.TCCRxA);
                            setDataByte<1>(timer4.TCCRxB);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer4.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer4.TCCRxB = getDataByte<1>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x30 + 2: { 
                        /* TCCRnC and Reserved (ignore that) */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<2>(timer4.TCCRxC);
                            setDataByte<3>(0); 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer4.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 0x30 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer4.TCNTx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer4.TCNTx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x30 + 6: { 
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer4.ICRx;
                            interrupts(); 
                            dataLinesHalves[1] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer4.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x30 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer4.OCRxA;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer4.OCRxA = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x30 + 10: {
                         /* OCRnB */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer4.OCRxB;
                             interrupts(); 
                             dataLinesHalves[1] = tmp; 
                         } else { 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer4.OCRxB = value; 
                                interrupts(); 
                             } 
                         } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 0x30 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer4.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer4.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
            case 0x30 + 14: { 
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                         break;
                     }  
#endif
#ifdef TCCR5A
            case 0x40 + 0: { 
                        /* TCCRnA and TCCRnB */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<0>(timer5.TCCRxA);
                            setDataByte<1>(timer5.TCCRxB);
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW) { 
                                timer5.TCCRxA = getDataByte<0>();
                            } 
                            if (digitalRead<Pin::BE1>() == LOW) { 
                                timer5.TCCRxB = getDataByte<1>();
                            } 
                        } 
                        if (isBurstLast()) {
                            break; 
                        }
                        signalReady<true>();  
                    } 
            case 0x40 + 2: { 
                        /* TCCRnC and Reserved (ignore that) */ 
                        if constexpr (isReadOperation) { 
                            setDataByte<2>(timer5.TCCRxC);
                            setDataByte<3>(0); 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW) { 
                                timer5.TCCRxC = getDataByte<2>();
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 0x40 + 4: { 
                        /* TCNTn should only be accessible if you do a full 16-bit
                         * write 
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer5.TCNTx;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW && 
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer5.TCNTx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x40 + 6: { 
                        /* ICRn should only be accessible if you do a full 16-bit
                         * write
                         */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer5.ICRx;
                            interrupts(); 
                            dataLinesHalves[1] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE2>() == LOW &&  
                                    digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer5.ICRx = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x40 + 8: { 
                        /* OCRnA should only be accessible if you do a full 16-bit write */ 
                        if constexpr (isReadOperation) { 
                            noInterrupts(); 
                            auto tmp = timer5.OCRxA;
                            interrupts(); 
                            dataLinesHalves[0] = tmp; 
                        } else { 
                            if (digitalRead<Pin::BE0>() == LOW &&  
                                    digitalRead<Pin::BE1>() == LOW) { 
                                auto value = dataLinesHalves[0]; 
                                noInterrupts(); 
                                timer5.OCRxA = value;
                                interrupts(); 
                            } 
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>(); 
                    } 
            case 0x40 + 10: {
                         /* OCRnB */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer5.OCRxB;
                             interrupts(); 
                             dataLinesHalves[1] = tmp; 
                         } else { 
                             if (digitalRead<Pin::BE2>() == LOW &&  
                                     digitalRead<Pin::BE3>() == LOW) { 
                                auto value = dataLinesHalves[1]; 
                                noInterrupts(); 
                                timer5.OCRxB = value; 
                                interrupts(); 
                             } 
                         } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 0x40 + 12: { 
                         /* OCRnC */ 
                         if constexpr (isReadOperation) { 
                             noInterrupts(); 
                             auto tmp = timer5.OCRxC; 
                             interrupts(); 
                             dataLinesHalves[0] = tmp; 
                         } else { 
                              if (digitalRead<Pin::BE0>() == LOW && 
                                      digitalRead<Pin::BE1>() == LOW) { 
                                  auto value = dataLinesHalves[0]; 
                                  noInterrupts(); 
                                  timer5.OCRxC = value;
                                  interrupts(); 
                              }
                         } 
                         if (isBurstLast()) {
                             break;
                         } 
                         signalReady<true>(); 
                     } 
            case 0x40 + 14: { 
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                            dataLinesHalves[1] = 0; 
                         } 
                         break;
                     }  
#endif
            default:
                     if constexpr (isReadOperation) {
                         dataLinesFull = 0;
                     }
                     idleTransaction();
                     return;
        } 
        signalReady<true>(); 
}
};



template<uint8_t value>
[[gnu::always_inline]]
inline 
void 
updateDataLinesDirection() noexcept {
    dataLinesDirection_bytes[0] = value;
    dataLinesDirection_bytes[1] = value;
    dataLinesDirection_bytes[2] = value;
    dataLinesDirection_bytes[3] = value;
}

template<NativeBusWidth width, bool isReadOperation>
FORCE_INLINE
inline
void
handleOperationProper() noexcept {
    if (!digitalRead<Pin::IsMemorySpaceOperation>()) {
        // io operation
        CommunicationKernel<isReadOperation, width>::doIO();
    } else {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        CommunicationKernel<isReadOperation, width>::doCommunication();
    }
}

template<NativeBusWidth width, bool currentlyRead>
FORCE_INLINE
inline
void handleFullOperationProper() noexcept {
    while (digitalRead<Pin::DEN>());
    startTransaction();
    //const uint16_t al = addressLinesLowerHalf;
    // okay so we know that we are going to write so don't query the
    // pin!
    
    if (EnableSpecialSpace && !digitalRead<Pin::SpecialSpace>()) {
        idleTransaction();
    } else {
        if (!digitalRead<Pin::ChangeDirection>()) {
            updateDataLinesDirection<currentlyRead ? 0 : 0xFF>();
            toggle<Pin::DirectionOutput>();
            handleOperationProper<width, !currentlyRead>();
        } else {
            handleOperationProper<width, currentlyRead>();
        }
    }
    endTransaction();
}
template<NativeBusWidth width> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    // turn off the timer0 interrupt for system count, we don't care about it
    // anymore
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
            handleFullOperationProper<width, true>();
        } else {
            // start in write
            handleFullOperationProper<width, false>();
            // currently a write operation
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
    SPI.begin();
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    SdFs SD;
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
    SPI.endTransaction();
    SPI.end();
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
    pinMode(Pin::SpecialSpace, INPUT_PULLUP);
    if constexpr (MCUHasDirectAccess) {
        pinMode(Pin::READY, OUTPUT);
        digitalWrite<Pin::READY, HIGH>();
    }
}
void
setup() {
    setupPins();
    Serial.begin(115200);
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
    installMemoryImage();
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

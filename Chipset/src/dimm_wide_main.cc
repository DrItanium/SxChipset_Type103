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
void putCPUInReset() noexcept;
void pullCPUOutOfReset() noexcept;
template<bool isReadOperation>
struct RWOperation final {
    static constexpr bool IsReadOperation = isReadOperation;
};

template<uint8_t index>
requires (index < 4)
inline void setDataByte(uint8_t value) noexcept {
    static_assert(index < 4, "Invalid index provided to setDataByte, must be less than 4");
    if constexpr (index < 4) {
        switch (index) {
            case 0: getOutputRegister<Port::D0_7>() = value; break;
            case 1: getOutputRegister<Port::D8_15>() = value; break;
            case 2: getOutputRegister<Port::D16_23>() = value; break;
            case 3: getOutputRegister<Port::D24_31>() = value; break;
        }
    }
}
[[gnu::always_inline]]
inline void setDataWord(uint8_t a, uint8_t b, uint8_t c, uint8_t d) noexcept {
    setDataByte<0>(a);
    setDataByte<1>(b);
    setDataByte<2>(c);
    setDataByte<3>(d);
}
[[gnu::always_inline]]
inline void setDataWord(uint32_t value) noexcept {
    setDataByte<0>(static_cast<uint8_t>(value));
    setDataByte<1>(static_cast<uint8_t>(value >> 8));
    setDataByte<2>(static_cast<uint8_t>(value >> 16));
    setDataByte<3>(static_cast<uint8_t>(value >> 24));
}


[[gnu::always_inline]]
inline void setDataWord(uint16_t lo, uint16_t hi) noexcept {
    setDataWord(static_cast<uint8_t>(lo), 
            static_cast<uint8_t>(lo >> 8),
            static_cast<uint8_t>(hi), 
            static_cast<uint8_t>(hi >> 8));
}

template<uint8_t index>
requires (index < 4)
inline uint8_t getDataByte() noexcept {
    static_assert(index < 4, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 4) {
        switch (index) {
            case 0: return getInputRegister<Port::D0_7>();
            case 1: return getInputRegister<Port::D8_15>();
            case 2: return getInputRegister<Port::D16_23>();
            case 3: return getInputRegister<Port::D24_31>();
        }
    } else {
        return 0;
    }
}

template<uint8_t index>
requires (index < 2)
inline uint16_t getDataHalf() noexcept {
    if constexpr (index < 2) {
        switch (index) {
            case 0: return makeWord(getDataByte<1>(), getDataByte<0>());
            case 1: return makeWord(getDataByte<3>(), getDataByte<2>());
        }
    } else {
        return 0;
    }
}

template<uint8_t index>
requires (index < 2)
inline void setDataHalf(uint16_t value) noexcept {
    if constexpr (index < 2) {
        switch (index) {
            case 0:
                setDataByte<0>(static_cast<uint8_t>(value));
                setDataByte<1>(static_cast<uint8_t>(value >> 8));
                break;
            case 1:
                setDataByte<2>(static_cast<uint8_t>(value));
                setDataByte<3>(static_cast<uint8_t>(value >> 8));
                break;
        }
    } 
}

template<uint8_t index>
requires (index < 2)
inline void setDataHalf(uint8_t lower, uint8_t upper) noexcept {
    if constexpr (index < 2) {
        switch (index) {
            case 0:
                setDataByte<0>(lower);
                setDataByte<1>(upper);
                break;
            case 1:
                setDataByte<2>(lower);
                setDataByte<3>(upper);
                break;
        }
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
static void doIO() noexcept { 
        switch (getInputRegister<Port::A2_9>()) {
            case 0: { 
                        if constexpr (isReadOperation) { 
                            setDataWord(F_CPU);
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            setDataWord(F_CPU / 2);
                        } 
                        if (isBurstLast()) { 
                            break; 
                        } 
                        signalReady<true>();  
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            setDataHalf<0>(Serial.read());
                            setDataHalf<1>(0);
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(static_cast<uint8_t>(getDataHalf<0>()));
                        } 
                         if (isBurstLast()) { 
                             break; 
                         } 
                         signalReady<true>(); 
                     } 
            case 12: { 
                         Serial.flush();
                         if constexpr (isReadOperation) { 
                             setDataWord(0);
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
                            setDataWord(obj.TCCRxA, obj.TCCRxB, obj.TCCRxC, 0);\
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
                            setDataWord(tmp, tmp2); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW && \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = getDataHalf<0>(); \
                                noInterrupts(); \
                                obj.TCNTx = value;\
                                interrupts(); \
                            } \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = getDataHalf<1>(); \
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
                            setDataWord(tmp, tmp2); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = getDataHalf<0>(); \
                                noInterrupts(); \
                                obj.OCRxA = value;\
                                interrupts(); \
                            } \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = getDataHalf<1>(); \
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
                             setDataWord(tmp, 0); \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = getDataHalf<0>(); \
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
                         setDataWord(0);
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
    // io operation
    doIO();
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
#define I960_Signal_Switch \
    if (isBurstLast()) { \
        break; \
    } \
    signalReady<true>()

FORCE_INLINE 
inline 
static void doIO() noexcept { 
        switch (getInputRegister<Port::A2_9>()) { 
            case 0: { 
                        if constexpr (isReadOperation) { 
                            setDataHalf<0>(static_cast<uint16_t>(F_CPU));
                        } 
                        I960_Signal_Switch;
                    } 
            case 2: { 
                        if constexpr (isReadOperation) { 
                            setDataHalf<1>(static_cast<uint16_t>(F_CPU >> 16));
                        } 
                        I960_Signal_Switch;
                    } 
            case 4: { 
                        if constexpr (isReadOperation) { 
                            setDataHalf<0>(static_cast<uint16_t>(F_CPU / 2));
                        } 
                        I960_Signal_Switch;
                    } 
            case 6: { 
                        if constexpr (isReadOperation) { 
                            setDataHalf<1>(static_cast<uint16_t>((F_CPU / 2)>> 16));
                        } 
                        I960_Signal_Switch;
                    } 
            case 8: { 
                        /* Serial RW connection */
                        if constexpr (isReadOperation) { 
                            setDataHalf<0>(Serial.read());
                        } else { 
                            // no need to check this out just ignore the byte
                            // enable lines
                            Serial.write(getDataByte<0>());
                        } 
                        I960_Signal_Switch;
                    } 
            case 10: {
                         if constexpr (isReadOperation) { 
                             setDataHalf<1>(0);
                         } 
                        I960_Signal_Switch;
                     } 
            case 12: { 
                         if constexpr (isReadOperation) { 
                             setDataHalf<0>(0);
                         } else { 
                             Serial.flush();
                         }
                         I960_Signal_Switch;
                     }
            case 14: {
                        /* nothing to do on writes but do update the data port
                         * on reads */ 
                         if constexpr (isReadOperation) { 
                             setDataHalf<1>(0);
                         } 
                     }
                     break;
#define X(obj, offset) \
            case offset + 0: { \
                        /* TCCRnA and TCCRnB */ \
                        if constexpr (isReadOperation) { \
                            setDataHalf<0>(obj.TCCRxA, obj.TCCRxB); \
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
                            setDataHalf<1>(obj.TCCRxC, 0); \
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
                            setDataHalf<0>(tmp); \
                        } else {  \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) {  \
                                auto value = getDataHalf<0>(); \
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
                            setDataHalf<1>(tmp); \
                        } else { \
                            if (digitalRead<Pin::BE2>() == LOW &&  \
                                    digitalRead<Pin::BE3>() == LOW) { \
                                auto value = getDataHalf<1>(); \
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
                            setDataHalf<0>(tmp); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = getDataHalf<0>(); \
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
                            setDataHalf<1>(tmp); \
                         } else { \
                             if (digitalRead<Pin::BE2>() == LOW &&  \
                                     digitalRead<Pin::BE3>() == LOW) { \
                                auto value = getDataHalf<1>(); \
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
                             setDataHalf<0>(tmp); \
                         } else { \
                              if (digitalRead<Pin::BE0>() == LOW && \
                                      digitalRead<Pin::BE1>() == LOW) { \
                                  auto value = getDataHalf<0>(); \
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
                             setDataHalf<1>(0); \
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
                         setDataWord(0);
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
    // io operation
    doIO();
}
};



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
    // at this point we want the code to just respond with non io operations
    while (true) {
        while (digitalRead<Pin::DEN>());
        if (digitalRead<Pin::IO_OPERATION>() == LOW) {
            // this is an io operation. So make sure that we have our data
            // lines in the correct direction
            if (digitalRead<Pin::WR>() == LOW) {
                // a read operation means output so 0xFF
                updateDataLinesDirection<0xFF>();
                // now we need to pull the data in and do the transaction as we
                // walk through
                CommunicationKernel<true, width>::dispatch();
            } else {
                // a write operation means input so 0
                updateDataLinesDirection<0>();
                CommunicationKernel<false, width>::dispatch();
            }
        } else {
            // it's not an IO operation so instead just do an "idle"
            // transaction
            idleTransaction();
        }
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
putCPUInReset() noexcept {
    auto updatedValue = ioExpRead8<Pin::IO_EXP_ENABLE>(0x12) & 0b0111'1111;
    ioExpWrite8<Pin::IO_EXP_ENABLE>(0x12, updatedValue);
}
void 
pullCPUOutOfReset() noexcept {
    /// @todo implement
    auto updatedValue = ioExpRead8<Pin::IO_EXP_ENABLE>(0x12) | 0b1000'0000;
    ioExpWrite8<Pin::IO_EXP_ENABLE>(0x12, updatedValue);
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

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
#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>


#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;

constexpr bool XINT1DirectConnect = false;
constexpr bool XINT2DirectConnect = false;
constexpr bool XINT3DirectConnect = false;
constexpr bool XINT4DirectConnect = false;
constexpr bool XINT5DirectConnect = false;
constexpr bool XINT6DirectConnect = false;
constexpr bool XINT7DirectConnect = false;
constexpr bool PrintBanner = true;
constexpr bool SupportNewRAMLayout = false;
constexpr auto TransferBufferSize = 16384;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;
constexpr bool PerformMemoryImageInstallation = true;

constexpr uintptr_t MemoryWindowBaseAddress = SupportNewRAMLayout ? 0x8000 : 0x4000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;

static_assert((( SupportNewRAMLayout && MemoryWindowMask == 0x7FFF) || (!SupportNewRAMLayout && MemoryWindowMask == 0x3FFF)), "MemoryWindowMask is not right");
using BusKind = AccessFromIBUS;
//using BusKind = AccessFromNewIBUS;


Adafruit_SSD1351 oled(
        128,
        128,
        &SPI, 
        EyeSpi::Pins::TFTCS,
        EyeSpi::Pins::DC,
        EyeSpi::Pins::Reset);


template<typename T>
struct OptionalDevice {
    constexpr bool found() const noexcept { return _found; }
    T* operator->() noexcept { return &_device; }
    const T* operator->() const noexcept { return &_device; }
    T& getDevice() noexcept { return _device; }
    const T& getDevice() const noexcept { return _device; }
    T& operator*() noexcept { return getDevice(); }
    const T& operator*() const noexcept { return getDevice(); }
    explicit constexpr operator bool() const noexcept { return _found; }
    template<typename ... Args>
    OptionalDevice(Args&& ... parameters) : _device(parameters...) { }
    void begin() noexcept {
        _found = _device.begin();
    }
    private:
        T _device;
        bool _found = false;
};



void
setupDisplay() noexcept {
    oled.begin();
    oled.setFont();
    oled.fillScreen(0);
    oled.setTextColor(0xFFFF);
    oled.setTextSize(1);
    oled.println(F("i960"));
    oled.enableDisplay(true);
}



void 
setupDevices() noexcept {
    setupDisplay();
}
[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
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
[[gnu::address(0x2200)]] volatile uint24_t addressLinesLower24;
// allocate 1024 bytes total
[[gnu::always_inline]] inline bool isBurstLast() noexcept { 
    return digitalRead<Pin::BLAST>() == LOW; 
}
[[gnu::always_inline]]
inline
void 
setBankIndex(uint8_t value) {
    getOutputRegister<Port::IBUS_Bank>() = value;
}

template<NativeBusWidth width>
inline constexpr uint8_t getWordByteOffset(uint8_t value) noexcept {
    return value & 0b1100;
}
constexpr
uint16_t
computeTransactionWindow(uint16_t offset) noexcept {
    return MemoryWindowBaseAddress | (offset & MemoryWindowMask);
}

template<bool enableDebug>
FORCE_INLINE
inline
DataRegister8
getTransactionWindow() noexcept {
    if constexpr (SupportNewRAMLayout) {
        auto result = getInputRegister<Port::BankCapture>();
        if constexpr (enableDebug) {
            Serial.printf(F("Bank Index: 0x%x\n"), result);
        }
        setBankIndex(result);
        return memoryPointer<uint8_t>(computeTransactionWindow(addressLinesLowerHalf));
    } else {
        SplitWord32 split{addressLinesLower24};
        setBankIndex(split.getBankIndex(BusKind{}));
        return memoryPointer<uint8_t>(computeTransactionWindow(split.halves[0]));
    }
}
struct PulseReadySignal final { };
struct ToggleReadySignal final { };
using ReadySignalStyle = ToggleReadySignal;
template<bool waitForReady, Pin targetPin, int delayAmount>
[[gnu::always_inline]] 
inline void 
signalReadyRaw(PulseReadySignal) noexcept {
    pulse<targetPin>();
    if constexpr (waitForReady) {
        // wait four cycles after to make sure that the ready signal has been
        // propagated to the i960
        insertCustomNopCount<delayAmount>();
    }
}

template<bool waitForReady, Pin targetPin, int delayAmount>
[[gnu::always_inline]] 
inline void 
signalReadyRaw(ToggleReadySignal) noexcept {
    toggle<targetPin>();
    if constexpr (waitForReady) {
        // wait four cycles after to make sure that the ready signal has been
        // propagated to the i960
        insertCustomNopCount<delayAmount>();
    }
}

template<bool waitForReady, int delayAmount = 4>
[[gnu::always_inline]]
inline void
signalReady() noexcept {
    signalReadyRaw<waitForReady, Pin::READY, delayAmount>(ReadySignalStyle{});
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
    digitalWrite<Pin::RESET, LOW>();
}
void 
pullCPUOutOfReset() noexcept {
    digitalWrite<Pin::RESET, HIGH>();
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
inline uint8_t getDataByte() noexcept {
    static_assert(index < 4, "Invalid index provided to getDataByte, must be less than 4");
    if constexpr (index < 4) {
        return dataLines[index];
    } else {
        return 0;
    }
}
template<uint8_t value, NativeBusWidth width>
[[gnu::always_inline]]
inline 
void 
updateDataLinesDirection() noexcept {
    switch (width) {
        case NativeBusWidth::Sixteen:
            dataLinesDirection_bytes[0] = value;
            dataLinesDirection_bytes[1] = value;
            break;
        default:
            dataLinesDirection_bytes[0] = value;
            dataLinesDirection_bytes[1] = value;
            dataLinesDirection_bytes[2] = value;
            dataLinesDirection_bytes[3] = value;
            break;
    }
}

template<bool isReadOperation, NativeBusWidth width, bool enableDebug>
struct CommunicationKernel {
    using Self = CommunicationKernel<isReadOperation, width, enableDebug>;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
template<bool delay>
FORCE_INLINE
inline
static void signalReady() noexcept {
    ::signalReady<delay>();
}
FORCE_INLINE
inline
static void idleTransaction() noexcept {
    do {
        if (isBurstLast()) {
            signalReady<true>();
            break;
        }
        signalReady<true>();
    } while (true);
}
FORCE_INLINE
inline
static void
doCommunication() noexcept {
        auto theBytes = getTransactionWindow<enableDebug>(); 
        if constexpr (isReadOperation) {
            DataRegister32 view32 = reinterpret_cast<DataRegister32>(theBytes);
            dataLinesFull = view32[0];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[1];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[2];
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            dataLinesFull = view32[3];
        } else {
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[0] = getDataByte<0>();
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[1] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[2] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[3] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte since we
            // flowed into this
            theBytes[4] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[5] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[6] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[7] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte since we
            // flowed into this
            theBytes[8] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[9] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[10] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[11] = getDataByte<3>();
            } 
            if (isBurstLast()) {
                goto Done;
            }
            signalReady<true>();
            // we know that we can safely write to the lowest byte!
            theBytes[12] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[13] = getDataByte<1>();
            }
            if (digitalRead<Pin::BE2>() == LOW) {
                theBytes[14] = getDataByte<2>();
            }
            if (digitalRead<Pin::BE3>() == LOW) {
                theBytes[15] = getDataByte<3>();
            } 
        }
Done:
        signalReady<true>();

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
};

template<int index>
constexpr auto ByteEnablePinDetect = Pin::Count;
template<> constexpr auto ByteEnablePinDetect<0> = Pin::BE0;
template<> constexpr auto ByteEnablePinDetect<1> = Pin::BE1;
template<> constexpr auto ByteEnablePinDetect<2> = Pin::BE2;
template<> constexpr auto ByteEnablePinDetect<3> = Pin::BE3;

template<bool isReadOperation, bool enableDebug>
struct CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen, enableDebug> {
    using Self = CommunicationKernel<isReadOperation, NativeBusWidth::Sixteen, enableDebug>;
    static constexpr auto BusWidth = NativeBusWidth::Sixteen;
    CommunicationKernel() = delete;
    ~CommunicationKernel() = delete;
    CommunicationKernel(const Self&) = delete;
    CommunicationKernel(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

public:
template<bool delay>
FORCE_INLINE
inline
static void signalReady() noexcept {
    ::signalReady<delay>();
}
FORCE_INLINE
inline
static void idleTransaction() noexcept {
    while (!isBurstLast()) {
        signalReady<true>();
    }
    signalReady<true>();
}
    FORCE_INLINE
    inline
    static void
    doCommunication() noexcept {
        // we don't need to worry about the upper 16-bits of the bus like we
        // used to. In this improved design, there is no need to keep track of
        // where we are starting. Instead, we can easily just do the check as
        // needed
        if constexpr (isReadOperation) {
            auto theWords = reinterpret_cast<DataRegister16>(getTransactionWindow<enableDebug>());
            dataLinesHalves[0] = theWords[0];
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            auto next = theWords[1];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[2];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[3];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[4];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[5];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[6];
            dataLinesHalves[0] = next;
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<false>();
            next = theWords[7];
            dataLinesHalves[0] = next;
        } else {
            auto theBytes = getTransactionWindow<enableDebug>(); 
            if (digitalRead<Pin::BE0>() == LOW) {
                theBytes[0] = getDataByte<0>();
            }
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[1] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[2] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[3] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[4] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[5] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[6] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[7] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[8] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[9] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[10] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[11] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[12] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[13] = getDataByte<1>();
            }
            if (isBurstLast()) { 
                goto Done; 
            } 
            signalReady<true>();
            theBytes[14] = getDataByte<0>();
            if (digitalRead<Pin::BE1>() == LOW) {
                theBytes[15] = getDataByte<1>();
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
                            dataLinesHalves[0] = static_cast<uint16_t>((F_CPU) >> 16);
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
                            dataLinesHalves[0] = static_cast<uint16_t>((F_CPU / 2) >> 16);
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
                             dataLinesHalves[0] = 0;
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
                            dataLinesHalves[0] = 0; 
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
                            setDataByte<0>(obj.TCCRxC);\
                            setDataByte<1>(0); \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW) { \
                                obj.TCCRxC = getDataByte<0>();\
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
                            dataLinesHalves[0] = tmp; \
                        } else { \
                            if (digitalRead<Pin::BE0>() == LOW &&  \
                                    digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
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
                             dataLinesHalves[0] = tmp; \
                         } else { \
                             if (digitalRead<Pin::BE0>() == LOW &&  \
                                     digitalRead<Pin::BE1>() == LOW) { \
                                auto value = dataLinesHalves[0]; \
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
                            dataLinesHalves[0] = 0; \
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
};
/*
void processPacketFromSender_Serial2(const PacketSerial& sender, const uint8_t* buffer, size_t size) {
    /// @todo implement
}
*/

template<bool isReadOperation, NativeBusWidth width, bool enableDebug>
FORCE_INLINE
inline
void
doIOOperation() noexcept {
    if (digitalRead<Pin::IsMemorySpaceOperation>()) {
        CommunicationKernel<isReadOperation, width, enableDebug>::doCommunication();
    } else {
        CommunicationKernel<isReadOperation, width, enableDebug>::doIO();
    }
}

template<NativeBusWidth width, bool enableDebug> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    digitalWrite<Pin::DirectionOutput, HIGH>();
    setBankIndex(0);
    static constexpr auto WaitPin = Pin::DEN;
    // this microcontroller is not responsible for signalling ready manually
    // in this method. Instead, an external piece of hardware known as "Timing
    // Circuit" in the Intel manuals handles all external timing. It is drawn
    // as a box which takes in the different enable signals and generates the
    // ready signal sent to the i960 based off of the inputs provided. It has
    // eluded me for a very long time. I finally realized what it acutually is,
    // a counter that is configured around delay times for non intelligent
    // devices (flash, sram, dram, etc) and a multiplexer to allow intelligent
    // devices to control the ready signal as well.
    //
    // In my design, this mysterious circuit is a GAL22V10 which takes in a
    // 10MHz clock signal and provides a 4-bit counter and multiplexer to
    // accelerate ready signal propagation and also tell the mega 2560 when to
    // respond to the i960. The ready signal is rerouted from direct connection
    // of the i960 to the GAL22V10. Right now, I have to waste an extra pin on
    // the 2560 for this but my plan is to connect this directly to the 22V10
    // instead. 
    //
    // I also have connected the DEN and space detect pins to this 22V10 to
    // complete the package. I am not using the least significant counter bit
    // and instead the next bit up to allow for proper delays. The IO pin is
    // used to activate the mega2560 instead of the DEN pin directly. This is
    // currently a separate pin. In the future, I will be making this
    // infinitely more flexible by rerouting the DEN and READY pins into
    // external hardware that allows me to directly control the design again if
    // I so desire. 
    //
    // This change can also allow me to use more than one microcontroller for
    // IO devices if I so desire :D. 
    //
    // This version of the handler method assumes that you are within the
    // 16-megabyte window in the i960's memory space where this microcontroller
    // lives. So we wait for the GAL22V10 to tell me it is good to go to
    // continue!
ReadOperationStart:
    // read operation
    // wait until DEN goes low
    while (digitalRead<WaitPin>());
    // standard read/write operation so do the normal dispatch
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change direction to input since we are doing read -> write
        updateDataLinesDirection<0, width>();
        // update the direction pin 
        toggle<Pin::DirectionOutput>();
        // then jump into the write loop
        goto WriteOperationBypass;
    } 
ReadOperationBypass:
    if constexpr (enableDebug) {
        Serial.printf(F("R (0x%lx)\n"), addressLinesValue32);
    }
    doIOOperation<true, width, enableDebug>();
    goto ReadOperationStart;
WriteOperationStart:
    // wait until DEN goes low
    while (digitalRead<WaitPin>());
    // standard read/write operation so do the normal dispatch
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change direction to input since we are doing read -> write
        updateDataLinesDirection<0xFF, width>();
        // update the direction pin 
        toggle<Pin::DirectionOutput>();
        // then jump into the write loop
        goto ReadOperationBypass;
    } 
WriteOperationBypass:
    if constexpr (enableDebug) {
        Serial.printf(F("W (0x%lx)\n"), addressLinesValue32);
    }
    doIOOperation<false, width, enableDebug>();
    goto WriteOperationStart;
}

template<uint32_t maxFileSize = MaximumBootImageFileSize, auto BufferSize = TransferBufferSize>
[[gnu::noinline]]
void
installMemoryImage() noexcept {
    static constexpr uint32_t MaximumFileSize = maxFileSize;
    SPI.beginTransaction(SPISettings(F_CPU / 2, MSBFIRST, SPI_MODE0)); // force to 10 MHz
    SdFs SD;
    Serial.println(F("Looking for an SD Card!"));
    {
        while (!SD.begin(static_cast<int>(Pin::SD_EN))) {
            Serial.println(F("NO SD CARD!"));
            delay(1000);
        }
    }
    Serial.println(F("SD CARD FOUND!"));
    static constexpr auto filePath = "prog.bin";
    // look for firmware.bin and open it readonly
    if (auto theFirmware = SD.open(filePath, FILE_READ); !theFirmware) {
        Serial.printf(F("Could not open %s for reading!"), filePath);
        while (true) {
            delay(1000);
        }
    } else if (theFirmware.size() > MaximumFileSize) {
        Serial.println(F("The firmware image is too large to fit in sram!"));
        while (true) {
            delay(1000);
        }
    } else {
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            // just modify the bank as we go along
            setBankIndex(view.getBankIndex(BusKind{}));
            auto* theBuffer = memoryPointer<uint8_t>(view.unalignedBankAddress(BusKind{}));
            theFirmware.read(const_cast<uint8_t*>(theBuffer), BufferSize);
            Serial.print(F("."));
        }
        Serial.println(F("DONE!"));
        theFirmware.close();
    }
    // okay so now end reading from the SD Card
    SD.end();
    SPI.endTransaction();
}
void 
setupPins() noexcept {
    // power down the ADC and USART3
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate ADC
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
    pinMode(Pin::DirectionOutput, OUTPUT);
    pinMode(Pin::AlignmentCheck, INPUT);
    pinMode(Pin::A23_960, INPUT);
    // we start with 0xFF for the direction output so reflect it here
    digitalWrite<Pin::DirectionOutput, HIGH>();
    pinMode(Pin::ChangeDirection, INPUT);
    getDirectionRegister<Port::BankCapture>() = 0;
    pinMode(Pin::HOLD, OUTPUT);
    digitalWrite<Pin::HOLD, LOW>();
    pinMode(Pin::HLDA, INPUT);
    pinMode(Pin::LOCK, INPUT);
    pinMode(Pin::FAIL, INPUT);
    pinMode(Pin::RESET, OUTPUT);
    digitalWrite<Pin::RESET, LOW>();
    pinMode(Pin::CFG0, INPUT);
    pinMode(Pin::CFG1, INPUT);
    pinMode(Pin::CFG2, INPUT);

    pinMode(Pin::BusQueryEnable, OUTPUT);
    digitalWrite<Pin::BusQueryEnable, HIGH>();
    // set these up ahead of time
    //pinMode(Pin::EN2560, INPUT);
    pinMode(Pin::READY, OUTPUT);
    digitalWrite<Pin::READY, HIGH>();
    // setup bank capture to read in address lines
    pinMode(Pin::DebugEnable, INPUT_PULLUP);
    pinMode(Pin::LED, OUTPUT);
    digitalWrite<Pin::LED, LOW>();
}
void
setupExternalBus() noexcept {
    // setup the EBI
    XMCRB=0b1'0000'000;
    if constexpr (SupportNewRAMLayout) {
        XMCRA=0b1'100'01'01;  
    } else {
        XMCRA=0b1'010'01'01;  
    }
    // we divide the sector limits so that it 0x2200-0x7FFF and 0x8000-0xFFFF
    // the single cycle wait state is necessary even with the AHC573s
    AddressLinesInterface.view32.direction = 0;
    DataLinesInterface.view32.direction = 0xFFFF'FFFF;
    DataLinesInterface.view32.data = 0;
}

void 
setupPlatform() noexcept {
    setupExternalBus();
    setupDevices();
}

CPUKind 
getInstalledCPUKind() noexcept { 
    return static_cast<CPUKind>((getInputRegister<Port::CTL960>() >> 5) & 0b111);
}
void
printlnBool(bool value) noexcept {
    if (value) {
        Serial.println(F("TRUE"));
    } else {
        Serial.println(F("FALSE"));
    }
}
void banner() noexcept;

void
setup() {
    int32_t seed = 0;
#define X(value) seed += analogRead(value) 
    X(A0); X(A1); X(A2); X(A3);
    X(A4); X(A5); X(A6); X(A7);
    X(A8); X(A9); X(A10); X(A11);
    X(A12); X(A13); X(A14); X(A15);
#undef X
    randomSeed(seed);
    Serial.begin(115200);
    //Serial2.begin(115200);
    //serial2PacketEncoder.setStream(&Serial2);
    //serial2PacketEncoder.setPacketHandler([](const uint8_t* buffer, size_t size) { processPacketFromSender_Serial2(serial2PacketEncoder, buffer, size); });
    SPI.begin();
    Wire.begin();
    setupPins();
    // setup the IO Expanders
    setupPlatform();
    putCPUInReset();
    if constexpr (PrintBanner) {
        banner();
    }
    // find firmware.bin and install it into the 512k block of memory
    if constexpr (PerformMemoryImageInstallation) {
        installMemoryImage();
    } else {
        delay(1000);
    }
    pullCPUOutOfReset();
}
template<bool enableDebug>
[[noreturn]]
void
detectAndDispatch() {
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            executionBody<NativeBusWidth::Sixteen, enableDebug>();
            break;
        default:
            Serial.println(F("Target CPU is not supported by this firmware!"));
            while(true);
            break;
    }
}

void 
loop() {
    if (digitalRead<Pin::DebugEnable>() == HIGH) {
        detectAndDispatch<false>();
    } else {
        detectAndDispatch<true>();
    }
}

template<typename T>
void printlnBool(const T& value) noexcept {
    printlnBool(value.found());
}

void
banner() noexcept {
    Serial.println(F("i960 Chipset"));
    Serial.println(F("(C) 2019-2023 Joshua Scoggins"));
    Serial.println(F("Variant: Type 103 Narrow Wide"));
    constexpr uint32_t IORamMax = MemoryWindowBaseAddress * 256ul; // 256 banks *
                                                                 // window size
    Serial.println(F("Features: "));
    Serial.println(F("Bank Switching Controlled By AVR"));
    Serial.print(F("Base Address of IO RAM Window: 0x"));
    Serial.println(MemoryWindowBaseAddress, HEX);
    Serial.print(F("Maximum IO RAM Available: "));
    Serial.print(IORamMax, DEC);
    Serial.println(F(" bytes"));
    Serial.print(F("Memory Mapping Mode: "));
    Serial.println(F("Directly Connected FLASH/SRAM/RAM + IO Space with Independent RAM Section"));
    Serial.print(F("Detected i960 CPU Kind: "));
    switch (getInstalledCPUKind()) {
        case CPUKind::Sx:
            Serial.println(F("Sx"));
            break;
        case CPUKind::Kx:
            Serial.println(F("Kx"));
            break;
        case CPUKind::Jx:
            Serial.println(F("Jx"));
            break;
        case CPUKind::Hx:
            Serial.println(F("Hx"));
            break;
        case CPUKind::Cx:
            Serial.println(F("Cx"));
            break;
        default:
            Serial.println(F("Unknown"));
            break;
    }
    Serial.print(F("Bus Width: "));
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            Serial.println(F("16-bit"));
            break;
        case NativeBusWidth::ThirtyTwo:
            Serial.println(F("32-bit"));
            break;
        default:
            Serial.println(F("Unknown (fallback to 32-bit)"));
            break;
    }
    Serial.print(F("MCU Debug: "));
    printlnBool(digitalRead<Pin::DebugEnable>() == LOW);
}


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
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <Adafruit_EPD.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <Adafruit_SI5351.h>
#include <RTClib.h>

#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"

using DataRegister8 = volatile uint8_t*;
using DataRegister16 = volatile uint16_t*;
using DataRegister32 = volatile uint32_t*;

enum class EnabledDisplays {
    SSD1351_OLED_128_x_128_1_5,
    SSD1680_EPaper_250_x_122_2_13,
    ILI9341_TFT_240_x_320_2_8_Capacitive_TS,
};
enum class DisplayKind {
    Unknown,
    TFT,
    OLED,
    EPaper,
};
constexpr auto ActiveDisplay = EnabledDisplays::SSD1351_OLED_128_x_128_1_5;
constexpr auto EPAPER_COLOR_BLACK = EPD_BLACK;
constexpr auto EPAPER_COLOR_RED = EPD_RED;
constexpr bool MCUHasDirectAccess = true;
constexpr bool XINT0DirectConnect = true;
constexpr bool XINT1DirectConnect = false;
constexpr bool XINT2DirectConnect = false;
constexpr bool XINT3DirectConnect = false;
constexpr bool XINT4DirectConnect = false;
constexpr bool XINT5DirectConnect = false;
constexpr bool XINT6DirectConnect = false;
constexpr bool XINT7DirectConnect = false;
constexpr bool PrintBanner = true;
constexpr bool SupportNewRAMLayout = false;
constexpr bool HybridWideMemorySupported = false;
constexpr auto TransferBufferSize = 16384;
constexpr auto MaximumBootImageFileSize = 1024ul * 1024ul;

constexpr uintptr_t MemoryWindowBaseAddress = SupportNewRAMLayout ? 0x8000 : 0x4000;
constexpr uintptr_t MemoryWindowMask = MemoryWindowBaseAddress - 1;

static_assert((MemoryWindowMask == 0x7FFF || MemoryWindowMask == 0x3FFF), "MemoryWindowMask is not right");

constexpr auto displayHasTouchScreen() noexcept {
    switch (ActiveDisplay) {
        case EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS:
            return true;
        default:
            return false;
    }
}

constexpr auto getDisplayTechnology() noexcept {
    switch (ActiveDisplay) {
        case EnabledDisplays::SSD1351_OLED_128_x_128_1_5:
            return DisplayKind::OLED;
        case EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS:
            return DisplayKind::TFT;
        case EnabledDisplays::SSD1680_EPaper_250_x_122_2_13:
            return DisplayKind::EPaper;
        default:
            return DisplayKind::Unknown;
    }
}

Adafruit_SSD1351 oled(
        128,
        128,
        &SPI, 
        EyeSpi::Pins::TFTCS,
        EyeSpi::Pins::DC,
        EyeSpi::Pins::Reset);

Adafruit_SSD1680 epaperDisplay213(
        250, 
        122,
        EyeSpi::Pins::DC, 
        EyeSpi::Pins::Reset,
        EyeSpi::Pins::TFTCS,
        EyeSpi::Pins::MEMCS,
        EyeSpi::Pins::Busy,
        &SPI);
Adafruit_ILI9341 tft_ILI9341(&SPI, 
        EyeSpi::Pins::DC, 
        EyeSpi::Pins::TFTCS, 
        EyeSpi::Pins::RST);
Adafruit_FT6206 ts;

RTC_PCF8523 rtc;
volatile bool rtcFound = false;
volatile bool rtcInitialized = false;

Adafruit_SI5351 clockgen;
volatile bool clockgenFound = false;

[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2210)]] inline volatile CH351 ControlSignals;
[[gnu::address(0x2218)]] inline volatile CH351 XBUSBankRegister;
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

[[gnu::used]]
FORCE_INLINE
inline
DataRegister8
getTransactionWindow() noexcept {
    if constexpr (SupportNewRAMLayout) {
        setBankIndex(getInputRegister<Port::BankCapture>());
        return memoryPointer<uint8_t>(computeTransactionWindow(addressLinesLowerHalf));
    } else {
        SplitWord32 split{addressLinesLower24};
        setBankIndex(split.getIBUSBankIndex());
        return memoryPointer<uint8_t>(computeTransactionWindow(split.halves[0]));
    }
}


template<bool waitForReady = false>
[[gnu::always_inline]] 
inline void 
signalReady() noexcept {
    //pulse<Pin::READY>();
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
    ControlSignals.ctl.reset = LOW;
}
void 
pullCPUOutOfReset() noexcept {
    ControlSignals.ctl.reset = HIGH;
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
        
        if constexpr (isReadOperation) {
            auto a = theBytes[0];
            auto b = theBytes[1];
            if (isBurstLast()) {
                uint8_t baseIndex = 0;
                if (digitalRead<Pin::AlignmentCheck>() != LOW) {
                    baseIndex = 2;
                }
                dataLines[baseIndex] = a;
                dataLines[baseIndex+1] = b;
                goto Done;
            } else {
                auto c = theBytes[2];
                auto d = theBytes[3];
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    dataLines[0] = a;
                    dataLines[1] = b;
                    signalReady<true>();
                    dataLines[2] = c;
                    dataLines[3] = d;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto e = theBytes[4];
                    auto f = theBytes[5];
                    signalReady<true>();
                    dataLines[0] = e;
                    dataLines[1] = f;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto g = theBytes[6];
                    auto h = theBytes[7];
                    signalReady<true>();
                    dataLines[2] = g;
                    dataLines[3] = h;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto i = theBytes[8];
                    auto j = theBytes[9];
                    signalReady<true>();
                    dataLines[0] = i;
                    dataLines[1] = j;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto k = theBytes[10];
                    auto l = theBytes[11];
                    signalReady<true>();
                    dataLines[2] = k;
                    dataLines[3] = l;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto m = theBytes[12];
                    auto n = theBytes[13];
                    signalReady<true>();
                    dataLines[0] = m;
                    dataLines[1] = n;
                    if (isBurstLast()) {
                        goto Done; 
                    }
                    auto o = theBytes[14];
                    auto p = theBytes[15];
                    signalReady<true>();
                    dataLines[2] = o;
                    dataLines[3] = p;
                    goto Done;
                } else {
                    setDataByte(c, d, a, b);
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
                    goto Done;
                }
            }
        } else {
            if (isBurstLast()) {
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    auto w = dataLines[0];
                    auto x = dataLines[1];
                    if (digitalRead<Pin::BE0>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    if (digitalRead<Pin::BE1>() == LOW) {
                        theBytes[1] = x;
                    }
                } else {
                    auto w = dataLines[2];
                    auto x = dataLines[3];
                    if (digitalRead<Pin::BE2>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[1] = x;
                    }
                }
                goto Done;
            } else {
                if (digitalRead<Pin::AlignmentCheck>() == LOW) {
                    // defer writes as much as possible
                    auto w = dataLines[0];
                    auto x = dataLines[1];
                    if (digitalRead<Pin::BE0>() == LOW) [[gnu::likely]] {
                        theBytes[0] = w;
                    } 
                    theBytes[1] = x;
                    // we only need to check to see if BE0 is enabled
                    // BE1 must be enabled since we are going to flow into
                    // the next byte
                    signalReady<true>();
                    auto y = dataLines[2];
                    auto z = dataLines[3];
                    theBytes[2] = y;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[3] = z;
                    }
                    if (isBurstLast()) {
                        // lower must be valid since we are flowing into the
                        // next 16-bit word
                        goto Done;
                    } 
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    theBytes[4] = dataLines[0];
                    auto b = dataLines[1];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[5] = b ;
                    }
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[6] = dataLines[2];
                    auto d = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[7] = d;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the enable lines
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto f = dataLines[1];
                    theBytes[8] = dataLines[0];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[9] = f;
                    }
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[10] = dataLines[2];
                    auto h = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[11] = h;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    theBytes[12] = dataLines[0];
                    auto j = dataLines[1];
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[13] = j;
                    }
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    if (isBurstLast()) [[gnu::unlikely]] {
                        goto Done;
                    }
                    signalReady<true>();
                    theBytes[14] = dataLines[2];
                    auto l = dataLines[3];
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[15] = l;
                    }
                    goto Done;
                } else {
                    auto a = dataLines[2];
                    auto b = dataLines[3];
                    if (digitalRead<Pin::BE2>() == LOW) [[gnu::likely]] {
                        theBytes[0] = a;
                    }
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[1] = b;
                    }
                    signalReady<true>();
                    auto c = dataLines[0];
                    auto d = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[2] = c;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[3] = d;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto e = dataLines[2];
                    auto f = dataLines[3];
                    theBytes[4] = e;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[5] = f;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto g = dataLines[0];
                    auto h = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[6] = g;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[7] = h;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto i = dataLines[2];
                    auto j = dataLines[3];
                    theBytes[8] = i;
                    if (digitalRead<Pin::BE3>() == LOW) [[gnu::likely]] {
                        theBytes[9] = j;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    }
                    signalReady<true>();
                    auto k = dataLines[0];
                    auto l = dataLines[1];
                    // lower must be valid since we are flowing into the
                    // next 16-bit word
                    theBytes[10] = k;
                    if (digitalRead<Pin::BE1>() == LOW) [[gnu::likely]] {
                        theBytes[11] = l;
                    }
                    if (isBurstLast()) {
                        goto Done;
                    } 
                    // we know that all of these entries must be valid so
                    // don't check the values
                    signalReady<true>();
                    // since this is a flow in from previous values we actually
                    // can eliminate checking as many pins as possible
                    auto m = dataLines[2];
                    auto n = dataLines[3];
                    theBytes[12] = m;
                    if (digitalRead<Pin::BE3>() == LOW) {
                        theBytes[13] = n;
                    }
                    goto Done;
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

template<NativeBusWidth width> 
//[[gnu::optimize("no-reorder-blocks")]]
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    digitalWrite<Pin::DirectionOutput, HIGH>();
    setBankIndex(0);
    // switch the XBUS bank mode to i960 instead of AVR
    // I want to use the upper four bits the XBUS address lines
    // while I can directly connect to the address lines, I want to test to
    // make sure that this works as well
    ControlSignals.ctl.bankSelect = 1;

    XBUSBankRegister.view32.data = 0;
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
ReadOperationStart:
    // wait until DEN goes low
    while (digitalRead<Pin::DEN>());
    // check to see if we need to change directions
    if (!digitalRead<Pin::ChangeDirection>()) {
        // change direction to input since we are doing read -> write
        updateDataLinesDirection<0>();
        // update the direction pin 
        toggle<Pin::DirectionOutput>();
        // then jump into the write loop
        goto WriteOperationBypass;
    }
ReadOperationBypass:
    // standard read operation so do the normal dispatch
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        if constexpr (HybridWideMemorySupported) {
            idleTransaction();
        } else {
            CommunicationKernel<true, width>::doCommunication();
        }
    } else {
        if (digitalRead<Pin::A23_960>()) {
            CommunicationKernel<true, width>::doCommunication();
        } else {
            // io operation
            CommunicationKernel<true, width>::doIO();
        }
    }
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
    if (digitalRead<Pin::IsMemorySpaceOperation>()) [[gnu::likely]] {
        // the IBUS is the window into the 32-bit bus that the i960 is
        // accessing from. Right now, it supports up to 4 megabytes of
        // space (repeating these 4 megabytes throughout the full
        // 32-bit space until we get to IO space)
        if constexpr (HybridWideMemorySupported) {
            idleTransaction();
        } else {
            CommunicationKernel<false, width>::doCommunication();
        }
    } else {
        if (digitalRead<Pin::A23_960>()) {
            CommunicationKernel<false, width>::doCommunication();
        } else {
            // io operation
            CommunicationKernel<false, width>::doIO();
        }
    }
    // restart the write loop
    goto WriteOperationStart;
    // we should never get here!
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
        Serial.println(F("TRANSFERRING!!"));
        for (uint32_t address = 0; address < theFirmware.size(); address += BufferSize) {
            SplitWord32 view{address};
            // just modify the bank as we go along
            setBankIndex(view.getIBUSBankIndex());
            auto* theBuffer = memoryPointer<uint8_t>(view.unalignedBankAddress(AccessFromIBUS{}));
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
    // power down the ADC, TWI, and USART3
    // currently we can't use them
    PRR0 = 0b0000'0001; // deactivate TWI and ADC
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
    if constexpr (MCUHasDirectAccess) {
        pinMode(Pin::READY, OUTPUT);
        digitalWrite<Pin::READY, HIGH>();
    }
    // setup bank capture to read in address lines
    getDirectionRegister<Port::BankCapture>() = 0;
}
template<typename T>
void
displayPrintln(T value) noexcept {
    if constexpr (ActiveDisplay == EnabledDisplays::SSD1351_OLED_128_x_128_1_5) {
        oled.println(value);
        oled.enableDisplay(true);
    } else if constexpr (ActiveDisplay == EnabledDisplays::SSD1680_EPaper_250_x_122_2_13) {
        epaperDisplay213.println(value);
        epaperDisplay213.display();
    } else if constexpr (ActiveDisplay == EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS) {
        tft_ILI9341.println(value);
    }
}


void
setupDisplay() noexcept {
    if constexpr (ActiveDisplay == EnabledDisplays::SSD1351_OLED_128_x_128_1_5) {
        oled.begin();
        oled.setFont();
        oled.fillScreen(0);
        oled.setTextColor(0xFFFF);
        oled.setTextSize(1);
    } else if constexpr (ActiveDisplay == EnabledDisplays::SSD1680_EPaper_250_x_122_2_13) {
        epaperDisplay213.begin();
        epaperDisplay213.clearBuffer();
        epaperDisplay213.setCursor(0, 0);
        epaperDisplay213.setTextColor(EPAPER_COLOR_BLACK);
        epaperDisplay213.setTextWrap(true);
        epaperDisplay213.setTextSize(2);
    } else if constexpr (ActiveDisplay == EnabledDisplays::ILI9341_TFT_240_x_320_2_8_Capacitive_TS) {
        ts.begin();
        tft_ILI9341.begin();
        tft_ILI9341.fillScreen(ILI9341_BLACK);
        tft_ILI9341.setCursor(0, 0);
        tft_ILI9341.setTextColor(ILI9341_WHITE);  
        tft_ILI9341.setTextSize(2);
    }
    displayPrintln(F("i960"));
}
void
setupRTC() noexcept {
    rtcFound = rtc.begin();
    if (rtcFound) {
        if (!rtc.initialized() || rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            rtcInitialized = false;
        } else {
            rtcInitialized = true;
        }
        rtc.start();
        // compensate for rtc drifiting (taken from the example program)
        float drift = 43;  // plus or minus over observation period - set to 0
                           // to cancel previous calibration
        float periodSeconds = (7 * 86400); // total observation period in
                                           // sections
        float deviationPPM = (drift / periodSeconds * 1'000'000); // deviation
                                                                  // in parts
                                                                  // per
                                                                  // million
        float driftUnit = 4.34; // use with offset mode PCF8523_TwoHours

        int offset = round(deviationPPM / driftUnit); 
        rtc.calibrate(PCF8523_TwoHours, offset); // perform calibration once
                                                 // drift (seconds) and
                                                 // observation period (seconds)
                                                 // are correct
    }
}
void
setupClockGenerator() noexcept {
    if (clockgen.begin() != ERROR_NONE) {
        clockgenFound = false;
    } else {
        clockgenFound = true;
    }
}
void 
setupPlatform() noexcept {
    static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;
    // setup the EBI
    XMCRB=0b1'0000'000;
    // use the upper and lower sector limits feature to accelerate IO space
    // 0x2200-0x3FFF is full speed, rest has a one cycle during read/write
    // strobe delay since SRAM is 55ns
    //
    // I am using an HC573 on the interface board so the single cycle delay
    // state is necessary! When I replace the interface chip with the
    // AHC573, I'll get rid of the single cycle delay from the lower sector
    XMCRA=0b1'010'01'01;  
    AddressLinesInterface.view32.direction = 0;
    DataLinesInterface.view32.direction = 0xFFFF'FFFF;
    DataLinesInterface.view32.data = 0;
    ControlSignals.view32.direction = ControlSignalDirection;
    if constexpr (MCUHasDirectAccess) {
        ControlSignals.view8.direction[1] &= 0b11101111;
    }
    if constexpr (XINT0DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11111110;
    }
    if constexpr (XINT1DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11111101;
    }
    if constexpr (XINT2DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11111011;
    }
    if constexpr (XINT3DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11110111;
    }
    if constexpr (XINT4DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11101111;
    }
    if constexpr (XINT5DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b11011111;
    }
    if constexpr (XINT6DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b10111111;
    }
    if constexpr (XINT7DirectConnect) {
        ControlSignals.view8.direction[2] &= 0b01111111;
    }
    ControlSignals.view32.data = 0;
    ControlSignals.ctl.hold = 0;
    ControlSignals.ctl.reset = 0; // put i960 in reset
    ControlSignals.ctl.backOff = 1;
    ControlSignals.ctl.ready = 0;
    ControlSignals.ctl.nmi = 1;
    ControlSignals.ctl.xint0 = 1;
    ControlSignals.ctl.xint1 = 1;
    ControlSignals.ctl.xint2 = 1;
    ControlSignals.ctl.xint3 = 1;
    ControlSignals.ctl.xint4 = 1;
    ControlSignals.ctl.xint5 = 1;
    ControlSignals.ctl.xint6 = 1;
    ControlSignals.ctl.xint7 = 1;
    // select the CH351 bank chip to go over the xbus address lines
    ControlSignals.ctl.bankSelect = 0;
    setupDisplay();
    setupRTC();
    setupClockGenerator();
}

CPUKind 
getInstalledCPUKind() noexcept { 
    return static_cast<CPUKind>(ControlSignals.ctl.cfg); 
}

void
banner() {
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
    if constexpr (HybridWideMemorySupported) {
        Serial.println(F("Directly Connected FLASH/SRAM/RAM + IO Space with Independent RAM Section"));
    } else {
        Serial.println(F("IO RAM Section mapped throughout i960 Memory Space"));
    }
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
    Serial.print(F("Has RTC: "));
    if (rtcFound) {
        Serial.println(F("TRUE"));
        if (rtcInitialized) {
            Serial.println(F("RTC was already initialized"));
        } else {
            Serial.println(F("RTC needed to be initialized!"));
        }
    } else {
        Serial.println(F("FALSE"));
    }
    Serial.print(F("Has Clock Generator (Si5351): "));
    if (clockgenFound) {
        Serial.println(F("TRUE"));
    } else {
        Serial.println(F("FALSE"));
    }
}

void
setup() {
    Serial.begin(115200);
    SPI.begin();
    Wire.begin();
    setupPins();
    // setup the IO Expanders
    setupPlatform();
    if constexpr (PrintBanner) {
        banner();
    }
    // find firmware.bin and install it into the 512k block of memory
    installMemoryImage();
    pullCPUOutOfReset();
}
void 
loop() {
    switch (getBusWidth(getInstalledCPUKind())) {
        case NativeBusWidth::Sixteen:
            executionBody<NativeBusWidth::Sixteen>();
            break;
        case NativeBusWidth::ThirtyTwo:
            executionBody<NativeBusWidth::ThirtyTwo>();
            break;
        default:
            executionBody<NativeBusWidth::Unknown>();
            break;
    }
}


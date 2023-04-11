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
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <RTClib.h>
#include <Adafruit_SI5351.h>
#include <Adafruit_seesaw.h>

#include "Types.h"
#include "Pinout.h"
#include "Peripheral.h"
#include "Setup.h"
#include "SerialDevice.h"
//#include "InfoDevice.h"
#include "TimerDevice.h"

SerialDevice theSerial;
TimerDevice timerInterface;

Adafruit_ILI9341 tft(
        static_cast<uint8_t>(Pin::TFTCS),
        static_cast<uint8_t>(Pin::TFTDC));

RTC_PCF8523 rtc;
bool rtcAvailable = false;
Adafruit_SI5351 clockGen;
bool clockGeneratorAvailable = false;
Adafruit_seesaw seesaw0;
bool seesaw0Available = false;
void
setupRTC() noexcept {
    rtcAvailable = rtc.begin();
    if (!rtcAvailable) {
        Serial.println(F("NO PCF8523 RTC found!"));
    } else {
        Serial.println(F("Found PCF8523 RTC!"));
        if (!rtc.initialized() || rtc.lostPower()) {
            Serial.println(F("RTC is not initialized, setting time!"));
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }

        rtc.start();
        Serial.println(F("Enabling calibration to prevent rtc drift of 43 seconds in 1 week!"));
        // taken from the pcf8523.ino example sketch:
        // The PCF8523 can be calibrated for:
        //        - Aging adjustment
        //        - Temperature compensation
        //        - Accuracy tuning
        // The offset mode to use, once every two hours or once every minute.
        // The offset Offset value from -64 to +63. See the Application Note for calculation of offset values.
        // https://www.nxp.com/docs/en/application-note/AN11247.pdf
        // The deviation in parts per million can be calculated over a period of observation. Both the drift (which can be negative)
        // and the observation period must be in seconds. For accuracy the variation should be observed over about 1 week.
        // Note: any previous calibration should cancelled prior to any new observation period.
        // Example - RTC gaining 43 seconds in 1 week
        float drift = 43; // seconds plus or minus over oservation period - set to 0 to cancel previous calibration.
        float period_sec = (7 * 86400);  // total obsevation period in seconds (86400 = seconds in 1 day:  7 days = (7 * 86400) seconds )
        float deviation_ppm = (drift / period_sec * 1000000); //  deviation in parts per million (μs)
        float drift_unit = 4.34; // use with offset mode PCF8523_TwoHours
                                 // float drift_unit = 4.069; //For corrections every min the drift_unit is 4.069 ppm (use with offset mode PCF8523_OneMinute)
        int offset = round(deviation_ppm / drift_unit);
        rtc.calibrate(PCF8523_TwoHours, offset); // Un-comment to perform calibration once drift (seconds) and observation period (seconds) are correct
    }
}

void
setupClockGenerator() noexcept {
    clockGeneratorAvailable = (clockGen.begin() == ERROR_NONE);
    if (clockGeneratorAvailable) {
        Serial.println(F("Si5351 Clock Generator detected!"));
    } else {
        Serial.println(F("No Si5351 Clock Generator detected!"));
    }
}

void
setupSeesaw0() noexcept {
    seesaw0Available = seesaw0.begin();
    if (!seesaw0Available) {
        Serial.println(F("seesaw0 not found!"));
    } else {
        Serial.println(F("seesaw0 found!"));
    }
}
void
setupSeesaws() noexcept {
    setupSeesaw0();
}

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
EndDeviceOperationsList(InfoDevice)
ConnectPeripheral(TargetPeripheral::Info, InfoDeviceOperations);

BeginDeviceOperationsList(DisplayDevice)
    RW, // for the serial aspects of this
    Flush,
    DisplayWidthHeight,
    Rotation,
    InvertDisplay,
    ScrollTo,
    SetScrollMargins,
    SetAddressWindow,
    ReadCommand8,
    CursorX,
    CursorY,
    CursorXY,
    DrawPixel,
    DrawFastVLine,
    DrawFastHLine,
    FillRect,
    FillScreen,
    DrawLine,
    DrawRect,
    DrawCircle,
    FillCircle,
    DrawTriangle,
    FillTriangle,
    DrawRoundRect,
    FillRoundRect,
    SetTextWrap,
    DrawChar_Square,
    DrawChar_Rectangle,
    SetTextSize_Square,
    SetTextSize_Rectangle,
    SetTextColor0,
    SetTextColor1,
    // Transaction parts
    StartWrite,
    WritePixel,
    WriteFillRect,
    WriteFastVLine,
    WriteFastHLine,
    WriteLine,
    EndWrite,
    /// @todo add drawBitmap support

EndDeviceOperationsList(DisplayDevice)

ConnectPeripheral(TargetPeripheral::Display, DisplayDeviceOperations);

template<bool inDebugMode, bool isReadOperation, NativeBusWidth width, TargetPeripheral p>
void sendOpcodeSize(uint8_t offset) noexcept {
    CommunicationKernel<inDebugMode, isReadOperation, width>::template doFixedCommunication<OpcodeCount_v<p>>(offset);
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
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
                    break;
                case K::Size:
                    sendOpcodeSize<inDebugMode, true, width, TargetPeripheral::Info>(offset);
                    break;
                case K::GetChipsetClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU>(offset);
                    break;
                case K::GetCPUClock:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<F_CPU/2>(offset);
                    break;
                default:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(offset);
                    break;
            }
            return;
        case TargetPeripheral::Serial:
            switch (getFunctionCode<TargetPeripheral::Serial>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Serial>;
                case K::Available:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
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
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(offset);
                    return;
            }
            break;
        case TargetPeripheral::Timer:
            switch (getFunctionCode<TargetPeripheral::Timer>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Timer>;
                case K::Available:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
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
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(offset);
                    return;

            }
            break;
        case TargetPeripheral::Display:
            switch (getFunctionCode<TargetPeripheral::Display>(function)) {
                using K = ConnectedOpcode_t<TargetPeripheral::Display>;
                case K::Available:
                case K::RW:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0xFFFF'FFFF>(offset);
                    return;
                case K::Size:
                    sendOpcodeSize<inDebugMode, true, width, TargetPeripheral::Display>(offset);
                    return;
                case K::DisplayWidthHeight:
                    body[0].halves[0] = tft.width();
                    body[0].halves[1] = tft.height();
                    break;
                case K::Rotation:
                    body.bytes[0] = tft.getRotation();
                    break;
                case K::ReadCommand8:
                    // use the offset in the instruction to determine where to
                    // place the result and what to request from the tft
                    // display
                    body.bytes[offset & 0b1111] = tft.readcommand8(offset);
                    break;
                case K::CursorX: 
                    body[0].halves[0] = tft.getCursorX(); 
                    break;
                case K::CursorY: 
                    body[0].halves[0] = tft.getCursorY(); 
                    break;
                case K::CursorXY: 
                    body[0].halves[0] = tft.getCursorX();
                    body[0].halves[1] = tft.getCursorY(); 
                    break;
                case K::Flush:
                    // fallthrough
                    tft.flush();
                default:
                    CommunicationKernel<inDebugMode, true, width>::template doFixedCommunication<0>(offset);
                    return;
            }
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
            switch (static_cast<SerialDeviceOperations>(function)) {
                case SerialDeviceOperations::RW:
                    Serial.write(static_cast<uint8_t>(body.bytes[0]));
                    break;
                case SerialDeviceOperations::Flush:
                    Serial.flush();
                    break;
                case SerialDeviceOperations::Baud:
                    theSerial.setBaudRate(body[0].full);
                    break;
                default:
                    break;
            }
            //theSerial.performWrite(function, offset, body);
            break;
        case TargetPeripheral::Timer:
            switch (static_cast<TimerDeviceOperations>(function)) {
                case TimerDeviceOperations::SystemTimerPrescalar:
                    timerInterface.setSystemTimerPrescalar(body.bytes[0]);
                    break;
                case TimerDeviceOperations::SystemTimerComparisonValue:
                    timerInterface.setSystemTimerComparisonValue(body.bytes[0]);
                    break;
                default:
                    break;
            }
            break;
        case TargetPeripheral::Display:
            switch (static_cast<DisplayDeviceOperations>(function)) {
                case DisplayDeviceOperations::SetScrollMargins:
                    tft.setScrollMargins(body[0].halves[0],
                            body[0].halves[1]);
                    break;
                case DisplayDeviceOperations::SetAddressWindow:
                    tft.setAddrWindow(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::ScrollTo:
                    tft.scrollTo(body[0].halves[0]);
                    break;
                case DisplayDeviceOperations::InvertDisplay:
                    tft.invertDisplay(body.bytes[0] != 0);
                    break;
                case DisplayDeviceOperations::Rotation:
                    tft.setRotation(body.bytes[0]);
                    break;
                case DisplayDeviceOperations::RW:
                    tft.print(static_cast<uint8_t>(body.bytes[0]));
                    break;
                case DisplayDeviceOperations::Flush:
                    tft.flush();
                    break;
                case DisplayDeviceOperations::DrawPixel:
                    tft.drawPixel(body[0].halves[0], body[0].halves[1], body[1].halves[0]);
                    break;
                case DisplayDeviceOperations::DrawFastHLine:
                    tft.drawFastHLine(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::DrawFastVLine:
                    tft.drawFastVLine(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::FillRect:
                    tft.fillRect( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0]);
                    break;

                case DisplayDeviceOperations::FillScreen:
                    tft.fillScreen(body[0].halves[0]);
                    break;
                case DisplayDeviceOperations::DrawLine:
                    tft.drawLine( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0]);
                    break;
                case DisplayDeviceOperations::DrawRect:
                    tft.drawRect( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0]);
                    break;
                case DisplayDeviceOperations::DrawCircle:
                    tft.drawCircle(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::FillCircle:
                    tft.fillCircle(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::DrawTriangle:
                    tft.drawTriangle( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1],
                            body[3].halves[0]);
                    break;
                case DisplayDeviceOperations::FillTriangle:
                    tft.fillTriangle( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1],
                            body[3].halves[0]);
                    break;
                case DisplayDeviceOperations::DrawRoundRect:
                    tft.drawRoundRect( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1]);
                    break;
                case DisplayDeviceOperations::FillRoundRect:
                    tft.fillRoundRect( body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1]);
                    break;
                case DisplayDeviceOperations::SetTextWrap:
                    tft.setTextWrap(body.bytes[0]);
                    break;
                case DisplayDeviceOperations::CursorX: 
                    tft.setCursor(body[0].halves[0], tft.getCursorY());
                    break;
                case DisplayDeviceOperations::CursorY: 
                    tft.setCursor(tft.getCursorX(), body[0].halves[0]);
                    break;
                case DisplayDeviceOperations::CursorXY:
                    tft.setCursor(body[0].halves[0], body[0].halves[1]);
                    break;
                case DisplayDeviceOperations::DrawChar_Square:
                    tft.drawChar(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1]);
                    break;
                case DisplayDeviceOperations::DrawChar_Rectangle:
                    tft.drawChar(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0],
                            body[2].halves[1],
                            body[3].halves[0]);
                    break;
                case DisplayDeviceOperations::SetTextSize_Square:
                    tft.setTextSize(body[0].halves[0]);
                    break;
                case DisplayDeviceOperations::SetTextSize_Rectangle:
                    tft.setTextSize(body[0].halves[0],
                                    body[0].halves[1]);
                    break;
                case DisplayDeviceOperations::SetTextColor0:
                    tft.setTextColor(body[0].halves[0]);
                    break;
                case DisplayDeviceOperations::SetTextColor1:
                    tft.setTextColor(body[0].halves[0], body[0].halves[1]);
                    break;
                case DisplayDeviceOperations::StartWrite:
                    tft.startWrite();
                    break;
                case DisplayDeviceOperations::WritePixel:
                    tft.writePixel(body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0]);
                    break;
                case DisplayDeviceOperations::WriteFillRect:
                    tft.writeFillRect(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0]);
                    break;
                case DisplayDeviceOperations::WriteFastVLine:
                    tft.writeFastVLine(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::WriteFastHLine:
                    tft.writeFastHLine(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1]);
                    break;
                case DisplayDeviceOperations::WriteLine:
                    tft.writeLine(
                            body[0].halves[0],
                            body[0].halves[1],
                            body[1].halves[0],
                            body[1].halves[1],
                            body[2].halves[0]);
                    break;
                case DisplayDeviceOperations::EndWrite:
                    tft.endWrite();
                    break;
                default:
                    break;
            }
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

[[gnu::always_inline]]
inline
void 
updateBank(uint32_t addr, typename TreatAsOnChipAccess::AccessMethod) noexcept {
    if constexpr (PortKUsedForIBUSBankTransfer) {
        PORTJ = PINK;
    } else {
        Platform::setBank(computeBankIndex(addr, typename TreatAsOnChipAccess::AccessMethod {}),
                typename TreatAsOnChipAccess::AccessMethod{});
    }
}


template<bool inDebugMode, NativeBusWidth width> 
[[gnu::noinline]]
[[noreturn]] 
void 
executionBody() noexcept {
    SplitWord128 operation;
    uint8_t currentDirection = dataLinesDirection_LSB;
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
                dataLinesDirection = 0;
                currentDirection = 0;
            }
            switch (majorCode) {
                case 0x00: 
                    updateBank(al, typename TreatAsOnChipAccess::AccessMethod{});
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
                dataLinesDirection_bytes[0] = currentDirection;
                dataLinesDirection_bytes[1] = currentDirection;
                dataLinesDirection_bytes[2] = currentDirection;
                dataLinesDirection_bytes[3] = currentDirection;
            }
            switch (majorCode) {
                case 0x00: 
                    updateBank(al, typename TreatAsOnChipAccess::AccessMethod{});
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
    DDRK = 0x00;
    PORTK = 0;
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
    tft.begin();
    auto x = tft.readcommand8(ILI9341_RDMODE);
    Serial.println(F("DISPLAY INFORMATION"));
    Serial.print(F("Display Power Mode: 0x")); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDMADCTL);
    Serial.print(F("MADCTL Mode: 0x")); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDPIXFMT);
    Serial.print(F("Pixel Format: 0x")); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDIMGFMT);
    Serial.print(F("Image Format: 0x")); Serial.println(x, HEX);
    x = tft.readcommand8(ILI9341_RDSELFDIAG);
    Serial.print(F("Self Diagnostic: 0x")); Serial.println(x, HEX);
    tft.fillScreen(ILI9341_BLACK);
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
    setupRTC();
    setupClockGenerator();
    setupSeesaws();
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



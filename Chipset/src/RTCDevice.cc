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
#include "Peripheral.h"
#include "RTClib.h"
#include "Pinout.h"
#include "RTCDevice.h"

bool
TimerDevice::init() noexcept {
    if (rtc.begin()) {
        if (!rtc.isrunning()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        available_ = true;
    } else {
        available_ = false;
    }
    // make sure that INT0 is enabled as an output. Make it high
    pinMode<Pin::INT0_960_>(OUTPUT);
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD) || defined(TYPE203_BOARD)
    // enable CTC (OCR2A) mode
    bitClear(TCCR2A, WGM20);
    bitSet(TCCR2A, WGM21);
    bitClear(TCCR2B, WGM22);
    // clear the timer counter
    bitClear(TCCR2B, CS20);
    bitClear(TCCR2B, CS21);
    bitClear(TCCR2B, CS22);
    TCNT2 = 0;
#endif
    digitalWrite<Pin::INT0_960_, HIGH>();
    return available_;
}

uint16_t 
TimerDevice::extendedRead(const Channel0Value& m0) const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (getCurrentOpcode()) {
        case TimerDeviceOperations::UnixTime:
            return unixtimeCopy_.retrieveHalf(getOffset());
        case TimerDeviceOperations::SystemTimerPrescalar:
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)  || defined(TYPE203_BOARD)
            return static_cast<uint16_t>(TCCR2B & 0b111);
#endif
        case TimerDeviceOperations::SystemTimerComparisonValue:
#if defined(TYPE103_BOARD) ||  defined(TYPE104_BOARD) || defined(TYPE203_BOARD)
            return static_cast<uint16_t>(OCR2A);
#endif
        default:
            return 0;
    }
}
void 
TimerDevice::extendedWrite(const Channel0Value& m0, uint16_t value) noexcept {
    // do nothing
    switch (getCurrentOpcode()) {
        case TimerDeviceOperations::SystemTimerPrescalar: 
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)  || defined(TYPE203_BOARD)
            {
                // enable toggle mode
                auto maskedValue = value & 0b111;
                if (value != 0) {
                    bitSet(TCCR2A, COM2A0);
                    bitClear(TCCR2A, COM2A1);
                } else {
                    bitClear(TCCR2A, COM2A0);
                    bitClear(TCCR2B, COM2A1);
                }
                uint8_t result = TCCR2B & 0b1111'1000;
                result |= static_cast<uint8_t>(maskedValue);
                TCCR2B = result;
                break;
            }
#endif
        case TimerDeviceOperations::SystemTimerComparisonValue:
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)  || defined(TYPE203_BOARD)
            OCR2A = static_cast<uint8_t>(value);
            break;
#endif
        default:
            break;
    }
    /// @todo update write operations so that we cache results and perform the
    /// writes at the end
}

void
TimerDevice::onStartTransaction(const SplitWord32 &addr) noexcept {
    // ahead of time, sample the unix time. It will be safe to do this since
    // the resolution is in seconds. Even if it does shift over it is okay!
    switch (getCurrentOpcode()) {
        case TimerDeviceOperations::UnixTime:
            unixtimeCopy_.full = available_ ? rtc.now().unixtime() : 0;
            break;
        default:
            break;
    }
}

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
#include "Peripheral.h"
#include "RTClib.h"
#include "Pinout.h"

namespace {
    uint16_t
    SystemTimer_getCompareValue(const SplitWord32&, const Channel0Value&, byte) noexcept {
        return static_cast<uint16_t>(OCR2A);
    }
    
    void
    SystemTimer_setCompareValue(const SplitWord32&, const Channel0Value&, byte, uint16_t value) noexcept {
        OCR2A = static_cast<uint8_t>(value);
    }
    uint16_t
    SystemTimer_getPrescalarValue(const SplitWord32&, const Channel0Value&, byte) noexcept {
        return static_cast<uint16_t>(TCCR2B & 0b111);
    }
    
    void
    SystemTimer_setPrescalarValue(const SplitWord32&, const Channel0Value&, byte, uint16_t value) noexcept {
        uint8_t result = TCCR2B & 0b1111'1000;
        result |= static_cast<uint8_t>(value & 0b111);
        TCCR2B = result;
    }
}
void 
TimerDevice::handleExtendedReadOperation(const SplitWord32& addr, TimerDeviceOperations value) noexcept {
    switch (value) {
        case TimerDeviceOperations::UnixTime:
            readOnlyDynamicValue(addr, rtc.now().unixtime());
            break;
        case TimerDeviceOperations::SystemTimerComparisonValue:
            genericIOHandler<true>(addr, SystemTimer_getCompareValue, SystemTimer_setCompareValue);
            break;
        case TimerDeviceOperations::SystemTimerPrescalar:
            genericIOHandler<true>(addr, SystemTimer_getPrescalarValue, SystemTimer_setPrescalarValue);
            break;
        default:
            genericIOHandler<true>(addr);
            break;
    }
}
void 
TimerDevice::handleExtendedWriteOperation(const SplitWord32& addr, TimerDeviceOperations value) noexcept {
    switch (value) {
        case TimerDeviceOperations::SystemTimerComparisonValue:
            genericIOHandler<false>(addr, SystemTimer_getCompareValue, SystemTimer_setCompareValue);
            break;
        case TimerDeviceOperations::SystemTimerPrescalar:
            genericIOHandler<false>(addr, SystemTimer_getPrescalarValue, SystemTimer_setPrescalarValue);
            break;
        default:
            genericIOHandler<false>(addr);
            break;
    }
}

bool
TimerDevice::begin() noexcept {
    if (rtc.begin()) {
        if (!rtc.isrunning()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        available_ = true;
    } else {
        available_ = false;
    }
    pinMode<Pin::INT0_>(OUTPUT);
    digitalWrite<Pin::INT0_, HIGH>();
    bitSet(TCCR2A, COM2A0);
    bitClear(TCCR2A, COM2A1);
    bitClear(TCCR2A, WGM20);
    bitSet(TCCR2A, WGM21);
    bitClear(TCCR2B, WGM22);
    TCNT2 = 0;
    return available_;
}

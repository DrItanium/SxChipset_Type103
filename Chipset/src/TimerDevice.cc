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
#include "Pinout.h"
#include "TimerDevice.h"

bool
TimerDevice::init() noexcept {
    // make sure that INT0 is enabled as an output. Make it high
#if 0
    pinMode<Pin::INT0_960_>(OUTPUT);
#endif
#if defined(TCCR2A) && defined(TCCR2B) && defined(TCNT2)
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
#if 0
    digitalWrite<Pin::INT0_960_, HIGH>();
#endif
    return true;
}

uint16_t 
TimerDevice::extendedRead() const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (getCurrentOpcode()) {
#if defined(TCCR2B)
        case TimerDeviceOperations::SystemTimerPrescalar:
            return static_cast<uint16_t>(TCCR2B & 0b111);
#endif
#if defined(OCR2A)
        case TimerDeviceOperations::SystemTimerComparisonValue:
            return static_cast<uint16_t>(OCR2A);
#endif
        default:
            return 0;
    }
}
void 
TimerDevice::extendedWrite(uint16_t value) noexcept {
    // do nothing
    switch (getCurrentOpcode()) {
#if defined(TCCR2A) && defined(TCCR2B)
        case TimerDeviceOperations::SystemTimerPrescalar:
            {
                // enable toggle mode
                auto maskedValue = value & 0b111;
                if (value != 0) {
                    bitSet(TCCR2A, COM2A0);
                    bitClear(TCCR2A, COM2A1);
                } else {
                    bitClear(TCCR2A, COM2A0);
                    bitClear(TCCR2A, COM2A1);
                }
                uint8_t result = TCCR2B & 0b1111'1000;
                result |= static_cast<uint8_t>(maskedValue);
                TCCR2B = result;
                break;
            }
#endif
#ifdef OCR2A
        case TimerDeviceOperations::SystemTimerComparisonValue:
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
    // do nothing right now
}

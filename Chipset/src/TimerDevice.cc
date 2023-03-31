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
#include "Setup.h"

bool
TimerDevice::init() noexcept {
    // make sure that INT0 is enabled as an output. Make it high
    pinMode<Pin::INT0_960_>(OUTPUT);
#if defined(TCCR2A) && defined(TCCR2B) && defined(TCNT2)
    // enable CTC (OCR2A) mode
    // right now, we are doing everything through the CH351 chips so do not use
    // the output pins, interrupts will have to be used instead!
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
    return true;
}

void
TimerDevice::extendedRead(TimerDeviceOperations opcode, const SplitWord32 addr, SplitWord128& instruction) const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (opcode) {
#if defined(TCCR2B)
        case TimerDeviceOperations::SystemTimerPrescalar:
            instruction[0].setWholeValue(static_cast<uint32_t>(TCCR2B & 0b111));
            break;
#endif
#if defined(OCR2A)
        case TimerDeviceOperations::SystemTimerComparisonValue:
            instruction[0].setWholeValue(static_cast<uint32_t>(OCR2A));
            break;
#endif
        default:
            break;
    }
}
void 
TimerDevice::extendedWrite(TimerDeviceOperations opcode, const SplitWord32 addr, const SplitWord128& instruction) noexcept {
    // do nothing
    switch (opcode) {
#if defined(TCCR2A) && defined(TCCR2B)
        case TimerDeviceOperations::SystemTimerPrescalar:
            {
                // Previously, we were using the compare output mode of Timer
                // 2 but with the use of the CH351s I have to use the interrupt
                // instead
                auto value = instruction.bytes[0];
                auto maskedValue = value & 0b111;
                // enable toggle mode
                if (maskedValue != 0) {
                    bitSet(TCCR2A, COM2A0);
                    bitClear(TCCR2A, COM2A1);
                } else {
                    bitClear(TCCR2A, COM2A0);
                    bitClear(TCCR2A, COM2A1);
                }
                // make sure we activate the prescalar value
                uint8_t result = TCCR2B & 0b1111'1000;
                result |= static_cast<uint8_t>(maskedValue);
                TCCR2B = result;
                break;
            }
#endif
#ifdef OCR2A
        case TimerDeviceOperations::SystemTimerComparisonValue:
            OCR2A = static_cast<uint8_t>(instruction.bytes[0]);
            break;
#endif
        default:
            break;
    }
}


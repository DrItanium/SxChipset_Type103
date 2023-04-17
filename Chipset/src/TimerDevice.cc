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
TimerDevice::begin() noexcept {
    // make sure that INT0 is enabled as an output. Make it high
    pinMode<Pin::INT0_960_>(OUTPUT);
    pinMode<Pin::Timer1>(OUTPUT);
#if defined(TCCR1A) && defined(TCCR1B) && defined(TCNT1)
    // enable CTC (OCR1A) mode
    // right now, we are doing everything through the CH351 chips so do not use
    // the output pins, interrupts will have to be used instead!
    bitClear(TCCR1A, WGM10);
    bitClear(TCCR1A, WGM11);
    bitClear(TCCR1B, WGM12);
    bitClear(TCCR1B, WGM13);
    // clear the timer counter
    bitClear(TCCR1B, CS10);
    bitClear(TCCR1B, CS11);
    bitClear(TCCR1B, CS12);
    TCNT1 = 0;
#endif
    digitalWrite<Pin::INT0_960_, HIGH>();
    digitalWrite<Pin::Timer1, HIGH>();
    return true;
}
void
TimerDevice::setSystemTimerPrescalar(uint8_t value) noexcept {
#if defined(TCCR1A) && defined(TCCR1B)
    // Previously, we were using the compare output mode of Timer
    // 2 but with the use of the CH351s I have to use the interrupt
    // instead
    auto maskedValue = value & 0b111;
    // enable toggle mode
    if (maskedValue != 0) {
        bitSet(TCCR1A, COM1A0);
        bitClear(TCCR1A, COM1A1);
        bitSet(TCCR1A, COM1B0);
        bitClear(TCCR1A, COM1B1);
    } else {
        bitClear(TCCR1A, COM1A0);
        bitClear(TCCR1A, COM1A1);
        bitClear(TCCR1A, COM1B0);
        bitClear(TCCR1A, COM1B1);
    }
    // make sure we activate the prescalar value
    uint8_t result = TCCR1B & 0b1111'1000;
    result |= static_cast<uint8_t>(maskedValue);
    TCCR1B = result;
#endif
}
void
TimerDevice::setSystemTimerComparisonValue(uint16_t value) noexcept {
#ifdef OCR1A
    OCR1A = value;
    OCR1B = value / 2;
#endif
}

uint8_t 
TimerDevice::getSystemTimerPrescalar() const noexcept {
#if defined(TCCR1B)
    return (TCCR1B & 0b111);
#else
    return 0;
#endif
}

uint16_t
TimerDevice::getSystemTimerComparisonValue() const noexcept {
#if defined(OCR1A)
    return OCR1A;
#else
    return 0;
#endif
}

void
TimerDevice::handleWriteOperations(const SplitWord128& result, uint8_t function, uint8_t offset) noexcept {
    using K = ConnectedOpcode_t<TargetPeripheral::Timer>;
    switch (getFunctionCode<TargetPeripheral::Timer>(function)) {
        case K::SystemTimerPrescalar:
            setSystemTimerPrescalar(result.bytes[0]);
            break;
        case K::SystemTimerComparisonValue:
            setSystemTimerComparisonValue(result[0].halves[0]);
            break;
        default:
            break;
    }
}

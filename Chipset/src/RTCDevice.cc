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
    class PrescalarHandler final : public TransactionInterface {
        public:
            ~PrescalarHandler() override = default;
            uint16_t read(const Channel0Value&) const noexcept override { 
                return static_cast<uint16_t>(TCCR2B & 0b111);
            }
            void write(const Channel0Value& m0, uint16_t value) noexcept override { 
                uint8_t result = TCCR2B & 0b1111'1000;
                result |= static_cast<uint8_t>(value & 0b111);
                TCCR2B = result;
            }
    };
    class CompareHandler final : public TransactionInterface {
        public:
            ~CompareHandler() override = default;
            uint16_t read(const Channel0Value& m0) const noexcept override { 
                return static_cast<uint16_t>(OCR2A);
            }
            void write(const Channel0Value& m0, uint16_t value) noexcept override { 
                OCR2A = static_cast<uint8_t>(value);
            }
    };
    PrescalarHandler prescalarHandler;
    CompareHandler compareHandler;
}
void
TimerDevice::handleExtendedOperation(const SplitWord32& addr, TimerDeviceOperations value, OperationHandlerUser fn) noexcept {
    switch (value) {
        case TimerDeviceOperations::UnixTime: {
                                                  ExpressUint32_t tmp{rtc.now().unixtime()};
                                                  fn(addr, tmp);
                                                  break;
                                              }
        case TimerDeviceOperations::SystemTimerComparisonValue:
            fn(addr, compareHandler);
            break;
        case TimerDeviceOperations::SystemTimerPrescalar:
            fn(addr, prescalarHandler);
            break;
        default:
            fn(addr, getNullHandler());
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
    // make sure that INT0 is enabled as an output. Make it high
    pinMode<Pin::INT0_>(OUTPUT);
    // enable toggle mode
    bitSet(TCCR2A, COM2A0);
    bitClear(TCCR2A, COM2A1);
    // enable CTC (OCR2A) mode
    bitClear(TCCR2A, WGM20);
    bitSet(TCCR2A, WGM21);
    bitClear(TCCR2B, WGM22);
    // clear the timer counter
    bitClear(TCCR2B, CS20);
    bitClear(TCCR2B, CS21);
    bitClear(TCCR2B, CS22);
    TCNT2 = 0;
    digitalWrite<Pin::INT0_, HIGH>();
    return available_;
}

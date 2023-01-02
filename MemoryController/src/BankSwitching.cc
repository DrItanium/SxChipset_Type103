/**
 * i960SxChipset_Type103
 * Copyright (c) 2023, Joshua Scoggins
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef I960_MEGA_MEMORY_CONTROLLER
#include "BankSelection.h"

namespace External328Bus {
    void setBank(uint8_t bank) noexcept {
        // set the upper
        digitalWrite(FakeA15, bank & 0b1 ? HIGH : LOW);
        PORTK = (bank >> 1) & 0b0111'1111;
        PORTF = 0;
    }
    void begin() noexcept {
        static constexpr int PinList[] {
            A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15
        };
        for (auto pin : PinList) {
            pinMode(pin, OUTPUT);
        }
        pinMode(FakeA15, OUTPUT);
        setBank(0);
    }
    void select() noexcept {
        digitalWrite(RealA15, HIGH);
    }
} // end namespace External328Bus
namespace InternalBus {
    void setBank(uint8_t bank) noexcept {
        digitalWrite(BANK0, bank & 0b0001 ? HIGH : LOW);
        digitalWrite(BANK1, bank & 0b0010 ? HIGH : LOW);
        digitalWrite(BANK2, bank & 0b0100 ? HIGH : LOW);
        digitalWrite(BANK3, bank & 0b1000 ? HIGH : LOW);
    }
    void begin() noexcept {
        pinMode(BANK0, OUTPUT);
        pinMode(BANK1, OUTPUT);
        pinMode(BANK2, OUTPUT);
        pinMode(BANK3, OUTPUT);
        setBank(0);
    }
    void select() noexcept {
        digitalWrite(RealA15, LOW);
    }
} // end namespace InternalBus
#endif


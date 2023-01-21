//
// Created by jwscoggins on 1/14/23.
//
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
#include "BankSelection.h"

namespace External328Bus {
    void setBank(uint8_t bank) noexcept {
        // the 328 Bus is messed up on the 203 board so only use the internal 512k sram
        // I put a hole in PORTK which prevents the offsets into the banks from being properly defined
#if 0
        // set the upper
        digitalWrite<Pin::FakeA15>(bank & 0b1 ? HIGH : LOW);
        PORTF = (bank >> 1)
        //PORTK = (bank >> 1) & 0b0111'1111;
        //PORTF = 0;
#endif
    }
    void begin() noexcept {
#if 0
        static constexpr Pin PinList[] {
            Pin::PortF0,
            Pin::PortF1,
            Pin::PortF2,
            Pin::PortF3,
            Pin::PortF4,
            Pin::PortF5,
            Pin::PortF6,
            Pin::PortF7,
            Pin::PortJ6,
            Pin::PortJ7,
            /// @todo add more signals if it makes sense, the upper 6 lines are tied to ground/externally broken out
        };
        for (auto pin : PinList) {
            pinMode(pin, OUTPUT);
        }
        pinMode<Pin::FakeA15>(OUTPUT);
        setBank(0);
#endif
    }
    void select() noexcept {
#if 0
        digitalWrite<Pin::RealA15, HIGH>();
#endif
    }
} // end namespace External328Bus
namespace InternalBus {
    void setBank(uint8_t bank) noexcept {
#if 0
        auto bankSelect = PORTL;
        bankSelect &= 0b00001111;
        bankSelect |= ((bank << 4) & 0b11110000);
        PORTL = bankSelect;
#else
        getOutputRegister<Port::L>() = ((bank << 4) & 0xF0);
#endif
    }
    void begin() noexcept {
        pinMode<Pin::InternalBank0>(OUTPUT);
        pinMode<Pin::InternalBank1>(OUTPUT);
        pinMode<Pin::InternalBank2>(OUTPUT);
        pinMode<Pin::InternalBank3>(OUTPUT);
        setBank(0);
    }
    void select() noexcept {
        digitalWrite<Pin::RealA15, LOW>();
    }
} // end namespace InternalBus

void
BankSwitcher::begin() noexcept {
    XMCRB=0b00000'001; // need 32k. one pin released
    //XMCRA=0b1'100'00'00; // put in one cycle wait states
    XMCRA=0b1'000'00'00; // put in zero cycle wait states
    pinMode<Pin::RealA15>(OUTPUT);
    InternalBus::begin();
    External328Bus::begin();
    setBank(0);
}

void
BankSwitcher::setBank(uint8_t bank) noexcept {
    if (bank != currentBank_) {
        if (bank < 16) {
            InternalBus::select();
            InternalBus::setBank(bank);
        } else {
            External328Bus::select();
            External328Bus::setBank(bank - 16);
        }
        currentBank_ = bank;
    }
}


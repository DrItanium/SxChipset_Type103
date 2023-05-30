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
#include "Detect.h"

#include "Setup.h"
#include "Types.h"
#include "Pinout.h"


namespace {
    static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;

}
void
Platform::begin() noexcept {
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
}

void 
Platform::doReset(decltype(LOW) value) noexcept {
    ControlSignals.ctl.reset = value;
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    ControlSignals.ctl.hold = value;
}

#define X(index)  \
void  \
Platform::signalXINT ## index () noexcept { \
    ControlSignals.ctl.xint ## index = 0; \
    ControlSignals.ctl.xint ## index = 1; \
} \
void \
Platform::toggleXINT ## index () noexcept { \
    ControlSignals.ctl.xint ## index = ~ControlSignals.ctl.xint ## index ; \
}
X(0);
X(1);
X(2);
X(3);
X(4);
X(5);
X(6);
X(7);
#undef X

void
Platform::signalNMI() noexcept {
    ControlSignals.ctl.nmi = 0;
    ControlSignals.ctl.nmi = 1;
}

void 
Platform::setBank(const SplitWord32& addr, AccessFromIBUS) noexcept {
    setBank(addr.getIBUSBankIndex(), AccessFromIBUS{});
}
void 
Platform::setBank(const SplitWord32& addr, AccessFromXBUS) noexcept {
    setBank(addr.full, AccessFromXBUS{});
}

void 
Platform::setBank(uint8_t bankId, AccessFromIBUS) noexcept {
    PORTJ = bankId;
}
void 
Platform::setBank(uint32_t bankAddress, AccessFromXBUS) noexcept {
    XBUSBankRegister.view32.data = bankAddress;
}
uint8_t 
Platform::getBank(AccessFromIBUS) noexcept {
    return getOutputRegister<Port::IBUS_Bank>();
}

volatile uint8_t*
Platform::viewAreaAsBytes(const SplitWord32& addr, AccessFromIBUS) noexcept {
    // support IBUS 
    return memoryPointer<uint8_t>(addr.unalignedBankAddress(AccessFromIBUS{}));
}

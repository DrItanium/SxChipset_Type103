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
#include "Peripheral.h"


namespace {
    static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;

}
void
Platform::begin() noexcept {
    static bool initialized_ = false;
    if (!initialized_) {
        initialized_ = true;
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
        volatile auto& proc = getProcessorInterface();
        proc.address_.view32.direction = 0;
        proc.dataLines_.view32.direction = 0xFFFF'FFFF;
        proc.dataLines_.view32.data = 0;
        proc.control_.view32.direction = ControlSignalDirection;
        if constexpr (MCUHasDirectAccess) {
            proc.control_.view8.direction[1] &= 0b11101111;
        }
        if constexpr (XINT0DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11111110;
        }
        if constexpr (XINT1DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11111101;
        }
        if constexpr (XINT2DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11111011;
        }
        if constexpr (XINT3DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11110111;
        }
        if constexpr (XINT4DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11101111;
        }
        if constexpr (XINT5DirectConnect) {
            proc.control_.view8.direction[2] &= 0b11011111;
        }
        if constexpr (XINT6DirectConnect) {
            proc.control_.view8.direction[2] &= 0b10111111;
        }
        if constexpr (XINT7DirectConnect) {
            proc.control_.view8.direction[2] &= 0b01111111;
        }
        proc.control_.view32.data = 0;
        proc.control_.ctl.hold = 0;
        proc.control_.ctl.reset = 0; // put i960 in reset
        proc.control_.ctl.backOff = 1;
        proc.control_.ctl.ready = 0;
        proc.control_.ctl.nmi = 1;
        proc.control_.ctl.xint0 = 1;
        proc.control_.ctl.xint1 = 1;
        proc.control_.ctl.xint2 = 1;
        proc.control_.ctl.xint3 = 1;
        proc.control_.ctl.xint4 = 1;
        proc.control_.ctl.xint5 = 1;
        proc.control_.ctl.xint6 = 1;
        proc.control_.ctl.xint7 = 1;
        proc.control_.ctl.bankSelect = 0;
    }
}

void 
Platform::doReset(decltype(LOW) value) noexcept {
    getProcessorInterface().control_.ctl.reset = value;
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    getProcessorInterface().control_.ctl.hold = value;
}

#define X(index)  \
void  \
Platform::signalXINT ## index () noexcept { \
    getProcessorInterface().control_.ctl.xint ## index  = 0; \
    getProcessorInterface().control_.ctl.xint ## index  = 1; \
} \
void \
Platform::toggleXINT ## index () noexcept { \
    getProcessorInterface().control_.ctl.xint ## index = ~ getProcessorInterface().control_.ctl.xint ## index ; \
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
    getProcessorInterface().control_.ctl.nmi = 0;
    getProcessorInterface().control_.ctl.nmi = 1;
}


volatile SplitWord32& 
Platform::getMemoryView(const SplitWord32& addr, AccessFromIBUS) noexcept {
    return memory<SplitWord32>(addr.alignedBankAddress(AccessFromIBUS{}));
}

volatile SplitWord32& 
Platform::getMemoryView(const SplitWord32& addr, AccessFromXBUS) noexcept {
    return memory<SplitWord32>(addr.alignedBankAddress(AccessFromXBUS{}));
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
    getProcessorInterface().bank_.view32.data = bankAddress;
}
uint8_t 
Platform::getBank(AccessFromIBUS) noexcept {
    return PORTJ;
}
uint32_t 
Platform::getBank(AccessFromXBUS) noexcept {
    return getProcessorInterface().bank_.view32.data;
}

volatile uint8_t*
Platform::viewAreaAsBytes(const SplitWord32& addr, AccessFromIBUS) noexcept {
    // support IBUS 
    return memoryPointer<uint8_t>(addr.unalignedBankAddress(AccessFromIBUS{}));
}

volatile uint8_t*
Platform::viewAreaAsBytes(const SplitWord32& addr, AccessFromXBUS) noexcept {
    // support XBUS 
    return memoryPointer<uint8_t>(addr.unalignedBankAddress(AccessFromXBUS{})); 
}

volatile SplitWord128& 
Platform::getTransactionWindow(const SplitWord32& addr, AccessFromIBUS) noexcept {
    return memory<SplitWord128>(addr.transactionAlignedBankAddress(AccessFromIBUS{}));
}

volatile SplitWord128& 
Platform::getTransactionWindow(const SplitWord32& addr, AccessFromXBUS) noexcept {
    return memory<SplitWord128>(addr.transactionAlignedBankAddress(AccessFromXBUS{}));
}

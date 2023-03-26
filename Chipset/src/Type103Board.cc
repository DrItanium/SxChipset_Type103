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
        configureDataLinesForRead();
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
        configureDataLinesForRead();
    }
}
uint32_t
Platform::getDataLines() noexcept {
    return getProcessorInterface().getDataLines();
}
void
Platform::setDataLines(uint32_t value) noexcept {
    getProcessorInterface().setDataLines(value);
}

void
Platform::waitForDataState() noexcept {
    if constexpr (MCUHasDirectAccess) {
        while (digitalRead<Pin::DEN>() == HIGH) {
            // yield time since we are waiting
            yield();
        }
    } else {
        getProcessorInterface().waitForDataState();
    }
}

uint32_t
Platform::readAddress() noexcept {
    return getProcessorInterface().getAddress();
}
void 
Platform::doReset(decltype(LOW) value) noexcept {
    getProcessorInterface().control_.ctl.reset = value;
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    getProcessorInterface().control_.ctl.hold = value;
}

void
Platform::signalReady() noexcept {
    if constexpr (MCUHasDirectAccess) {
        toggle<Pin::READY>();
    } else {
        getProcessorInterface().control_.ctl.ready = ~getProcessorInterface().control_.ctl.ready;
    }

}

bool
Platform::isWriteOperation() noexcept {
    if constexpr (MCUHasDirectAccess) {
        return digitalRead<Pin::WR>();
    } else {
        return getProcessorInterface().control_.ctl.wr;
    }
}

bool
Platform::isIOOperation() noexcept {
    return getProcessorInterface().address_.view8.data[3] == 0xF0;
}

void
Platform::configureDataLinesForRead() noexcept {
    // output
    getProcessorInterface().dataLines_.view32.direction = 0xFFFF'FFFF;
}

void
Platform::configureDataLinesForWrite() noexcept {
    // input
    getProcessorInterface().dataLines_.view32.direction = 0;
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

bool
Platform::isBurstLast() noexcept {
    if constexpr (MCUHasDirectAccess) {
        return digitalRead<Pin::BLAST>() == 0;
    } else {
        return getProcessorInterface().control_.ctl.blast == 0;
    }
}

uint8_t
Platform::getByteEnable() noexcept {
    if constexpr (MCUHasDirectAccess) {
        return getInputRegister<Port::SignalCTL>() & 0b1111;
    } else {
        return getProcessorInterface().control_.ctl.byteEnable;
    }
}

uint16_t 
Platform::getUpperDataBits() noexcept {
    return getProcessorInterface().dataLines_.view16.data[1];
}
uint16_t 
Platform::getLowerDataBits() noexcept {
    return getProcessorInterface().dataLines_.view16.data[0];
}
void 
Platform::setUpperDataBits(uint16_t value) noexcept {

    getProcessorInterface().dataLines_.view16.data[1] = value;
}
void 
Platform::setLowerDataBits(uint16_t value) noexcept {

    getProcessorInterface().dataLines_.view16.data[0] = value;
}
void 
Platform::setDataByte(uint8_t index, uint8_t value) noexcept {
    getProcessorInterface().dataLines_.view8.data[index & 0b11] = value;
}
uint8_t 
Platform::getDataByte(uint8_t index) noexcept {
    return getProcessorInterface().dataLines_.view8.data[index & 0b11];
}

volatile SplitWord32& 
Platform::getMemoryView(const SplitWord32& addr, AccessFromIBUS) noexcept {
    return memory<SplitWord32>(addr.alignedBankAddress(AccessFromIBUS{}));
}

volatile SplitWord32& 
Platform::getMemoryView(const SplitWord32& addr, AccessFromXBUS) noexcept {
    return memory<SplitWord32>(addr.alignedBankAddress(AccessFromXBUS{}));
}
[[gnu::noinline]]
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
uint8_t
Platform::getAddressLSB() noexcept {
    return getProcessorInterface().address_.view8.data[0];
}

uint8_t
Platform::getAddressOffset() noexcept {
    return getAddressLSB() & 0b1111;
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

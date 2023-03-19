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
//#include "MCP23S17.h"
#include "Peripheral.h"
#include "BankSelection.h"


namespace {
    static constexpr uint32_t ControlSignalDirection = 0b10000000'11111111'00111000'00010001;
}
void
Platform::begin() noexcept {
    if (!initialized_) {
        initialized_ = true;
        volatile auto& proc = getProcessorInterface();
        proc.address_.view32.direction = 0;
        configureDataLinesForRead();
        proc.dataLines_.view32.data = 0;
        proc.control_.view32.direction = ControlSignalDirection;
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
        proc.bank_.view32.direction = 0xFFFF'FFFF;
        proc.bank_.view32.data = 0;
        BankSwitcher::begin();
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
    getProcessorInterface().waitForDataState();
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
    volatile auto& proc = getProcessorInterface();
    proc.control_.ctl.ready = ~proc.control_.ctl.ready;
}

bool
Platform::isReadOperation() noexcept {
    return getProcessorInterface().control_.ctl.wr == 0;
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

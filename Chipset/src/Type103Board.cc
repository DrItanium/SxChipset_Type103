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
#include "MCP23S17.h"
#include "Peripheral.h"
#include "BankSelection.h"

constexpr auto DataLines = MCP23S17::HardwareDeviceAddress::Device0;
/**
 * @brief Onboard device to control reset and other various features
 */
constexpr auto XIO = MCP23S17::HardwareDeviceAddress::Device7;


void
Platform::begin() noexcept {
    if (!initialized_) {
        initialized_ = true;
        // configure pins
        //pinMode<Pin::GPIOSelect>(OUTPUT);
        //pinMode<Pin::SD_EN>(OUTPUT);
        //pinMode<Pin::Ready>(OUTPUT);
        //pinMode<Pin::INT0_960_>(OUTPUT);
        //pinMode<Pin::Enable>(OUTPUT);
        //pinMode<Pin::CLKSignal>(OUTPUT);
        //pinMode<Pin::DEN>(INPUT);
        //pinMode<Pin::BLAST_>(INPUT);
        //pinMode<Pin::FAIL>(INPUT);
        //pinMode<Pin::Capture0>(INPUT);
        //pinMode<Pin::Capture1>(INPUT);
        //pinMode<Pin::Capture2>(INPUT);
        //pinMode<Pin::Capture3>(INPUT);
        //pinMode<Pin::Capture4>(INPUT);
        //pinMode<Pin::Capture5>(INPUT);
        //pinMode<Pin::Capture6>(INPUT);
        //pinMode<Pin::Capture7>(INPUT);
        //digitalWrite<Pin::CLKSignal, LOW>();
        //digitalWrite<Pin::Ready, HIGH>();
        //digitalWrite<Pin::GPIOSelect, HIGH>();
        //digitalWrite<Pin::INT0_960_, HIGH>();
        //digitalWrite<Pin::SD_EN, HIGH>();
        //digitalWrite<Pin::Enable, HIGH>();
        //// do an initial clear of the clock signal
        //pulse<Pin::CLKSignal, LOW, HIGH>();

        //BankSwitcher::begin();
        //getDirectionRegister<Port::DataLower>() = 0;
        //getDirectionRegister<Port::DataUpper>() = 0;
        //pinMode<Pin::AddressCaptureSignal1>(OUTPUT);
        //pinMode<Pin::AddressCaptureSignal2>(OUTPUT);
        //digitalWrite<Pin::AddressCaptureSignal1, HIGH>();
        //digitalWrite<Pin::AddressCaptureSignal2, HIGH>();
    }
}
struct [[gnu::packed]] CH351 {
    SplitWord32 value_;
    SplitWord32 direction_;
};
struct [[gnu::packed]] ProcessorInterface {
    CH351 address_;
    CH351 dataLines_;
    union {
        CH351 device;
        struct {
            uint8_t hold_ : 1;
            uint8_t hlda_ : 1;
            uint8_t lock_ : 1;
            uint8_t fail_ : 1;
            uint8_t reset_ : 1;
            uint8_t cfg_ : 3;
            uint8_t freq_ : 3;
            uint8_t backOff_ : 1;
            uint8_t ready_ : 1;
            uint8_t nmi_ : 1;
            uint8_t unused_ : 2;
            uint8_t xint0_ : 1;
            uint8_t xint1_ : 1;
            uint8_t xint2_ : 1;
            uint8_t xint3_ : 1;
            uint8_t xint4_ : 1;
            uint8_t xint5_ : 1;
            uint8_t xint6_ : 1;
            uint8_t xint7_ : 1;
            uint8_t byteEnable_ : 4;
            uint8_t den_ : 1;
            uint8_t blast_ : 1;
            uint8_t wr_ : 1;
            uint8_t bankSelect_ : 1;
        } bits_;
    } ctl;
    CH351 bank_;
    void waitForDataState() noexcept {
        while (ctl.bits_.den_ != 0);
    }
} ;
volatile ProcessorInterface&
getProcessorInterface() noexcept {
    return adjustedMemory<ProcessorInterface>(0);
}
uint32_t
Platform::getDataLines() noexcept {
    return getProcessorInterface().dataLines_.value_.getWholeValue();
}
void
Platform::setDataLines(uint32_t value) noexcept {
    getProcessorInterface().dataLines_.value_.full = value;
}

void
Platform::waitForDataState() noexcept {
    getProcessorInterface().waitForDataState();
}

uint8_t
Platform::readInputSignalPort() noexcept {
    return getProcessorInterface().ctl.device.value_.bytes[3];
}

uint32_t
Platform::readAddress() noexcept {
    return getProcessorInterface().address_.value_.getWholeValue();
}
void 
Platform::doReset(decltype(LOW) value) noexcept {
    getProcessorInterface().ctl.bits_.reset_ = value;
}
void 
Platform::doHold(decltype(LOW) value) noexcept {
    getProcessorInterface().ctl.bits_.hold_ = value;
}

void
Platform::signalReady() noexcept {
    readyStatus_ = !readyStatus_;
    getProcessorInterface().ctl.bits_.ready_ = readyStatus_;
}

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

namespace {
    constexpr auto SystemClockRate = F_CPU;
    constexpr auto CPUClockRate = SystemClockRate / 2;
    ExpressUint32_t systemRateExpose{0, SystemClockRate};
    ExpressUint32_t cpuClockRateExpose {0, CPUClockRate};
}
void 
InfoDevice::handleExtendedReadOperation(const SplitWord32& addr, InfoDeviceOperations value, OperationHandlerUser fn) noexcept {
    switch (value) {
        case InfoDeviceOperations::GetCPUClock: 
            fn(cpuClockRateExpose);
            break;
        case InfoDeviceOperations::GetChipsetClock:
            fn(systemRateExpose);
            break;
        default:
            fn(getNullHandler());
            break;
    }
}
void 
InfoDevice::handleExtendedWriteOperation(const SplitWord32& addr, InfoDeviceOperations value, OperationHandlerUser fn) noexcept {
    switch (value) {
        case InfoDeviceOperations::GetCPUClock:
            fn(cpuClockRateExpose);
            break;
        case InfoDeviceOperations::GetChipsetClock:
            fn(systemRateExpose);
            break;
        default:
            fn(getNullHandler());
            break;
    }
}

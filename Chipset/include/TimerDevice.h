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

#ifndef CHIPSET_TIMERDEVICE_H
#define CHIPSET_TIMERDEVICE_H
#include <Arduino.h>
#include "Types.h"
#include "Detect.h"
#include "Peripheral.h"
#include "IOOpcodes.h"


class TimerDevice {
public:
    bool begin() noexcept ;
    bool isAvailable() const noexcept { return true; }
    [[nodiscard]] uint8_t getSystemTimerPrescalar() const noexcept;
    [[nodiscard]] uint16_t getSystemTimerComparisonValue() const noexcept;
    void setSystemTimerPrescalar(uint8_t value) noexcept;
    void setSystemTimerComparisonValue(uint16_t value) noexcept;
    template<IOOpcodes opcode>
    void handleWriteOperations(const SplitWord128& result) noexcept {
        using K = IOOpcodes;
        switch (opcode) {
            case K::Timer_SystemTimer_Prescalar:
                setSystemTimerPrescalar(result.bytes[0]);
                break;
            case K::Timer_SystemTimer_CompareValue:
                setSystemTimerComparisonValue(result[0].halves[0]);
                break;
            default:
                break;
        }
    }
};
#endif //CHIPSET_TIMERDEVICE_H

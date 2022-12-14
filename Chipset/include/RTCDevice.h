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

#ifndef CHIPSET_RTCDEVICE_H
#define CHIPSET_RTCDEVICE_H
#include <Arduino.h>
#include "Types.h"
#include "Detect.h"
#include "RTClib.h"
#include "Peripheral.h"


BeginDeviceOperationsList(TimerDevice)
    UnixTime,
    SystemTimerComparisonValue,
    SystemTimerPrescalar,
EndDeviceOperationsList(TimerDevice)

class TimerDevice : public OperatorPeripheral<TimerDeviceOperations, TimerDevice> {
public:
    using Parent = OperatorPeripheral<TimerDeviceOperations, TimerDevice>;
    bool init() noexcept ;
    bool isAvailable() const noexcept { return available_; }
    void onStartTransaction(const SplitWord32& addr) noexcept;
    void onEndTransaction() noexcept { }
    uint16_t extendedRead(const Channel0Value& m0) const noexcept ;
    void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept ;
private:
    RTC_DS1307 rtc;
    bool available_ = false;
    SplitWord32 unixtimeCopy_{0};
};
#endif //CHIPSET_RTCDEVICE_H

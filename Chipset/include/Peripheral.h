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

#ifndef SXCHIPSET_TYPE103_PERIPHERAL_H
#define SXCHIPSET_TYPE103_PERIPHERAL_H
#include <Arduino.h>
#include "Detect.h"
#include "Types.h"
#include "Pinout.h"
#include "Setup.h"


enum class TargetPeripheral {
    Info, 
    Serial,
    Timer,
    Display,
    RTC,
    ClockGenerator,
    Count,
};
static_assert(static_cast<byte>(TargetPeripheral::Count) <= 256, "Too many Peripheral devices!");

template<TargetPeripheral p>
struct PeripheralDescription {
    using Self = PeripheralDescription<p>;
    PeripheralDescription() = delete;
    ~PeripheralDescription() = delete;
    PeripheralDescription(const Self&) = delete;
    PeripheralDescription(Self&&) = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;
};

#define ConnectPeripheral(p, op) \
    template<> \
    struct PeripheralDescription< p > { \
    using Self = PeripheralDescription<p>; \
    PeripheralDescription() = delete; \
    ~PeripheralDescription() = delete; \
    PeripheralDescription(const Self&) = delete; \
    PeripheralDescription(Self&&) = delete; \
    Self& operator=(const Self&) = delete; \
    Self& operator=(Self&&) = delete; \
        using OpcodeType = op ; \
    }

template<TargetPeripheral p>
using ConnectedOpcode_t = typename PeripheralDescription<p>::OpcodeType;

template<TargetPeripheral p>
constexpr auto getFunctionCode(uint8_t value) noexcept {
    return static_cast<ConnectedOpcode_t<p>>(value);
}

#define BeginDeviceOperationsList(name) enum class name ## Operations : byte { Available, Size,
#define EndDeviceOperationsList(name) Count, }; static_assert(static_cast<byte>( name ## Operations :: Count ) <= 256, "Too many " #name "Operations entries defined!");
#endif // end SXCHIPSET_TYPE103_PERIPHERAL_H

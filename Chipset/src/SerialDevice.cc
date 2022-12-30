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
#include "Wire.h"
uint16_t
performSerialRead_Fast(const SplitWord32&, const Channel0Value&, byte) noexcept {
    auto result = Serial.read();
    Wire.beginTransmission(8);
    Wire.write(0);
    Wire.write(static_cast<uint8_t>(result));
    Wire.endTransmission();
    return result;
}

void
performSerialWrite_Fast(const SplitWord32&, const Channel0Value&, byte, uint16_t value) noexcept {
    auto theChar = static_cast<uint8_t>(value);
    Serial.write(theChar);
    Wire.beginTransmission(8);
    Wire.write(0);
    Wire.write(theChar);
    Wire.endTransmission();
}

void 
SerialDevice::setBaudRate(uint32_t baudRate) noexcept { 
    if (baudRate != baud_) {
        baud_ = baudRate; 
        Serial.end();
        Serial.begin(baud_);
    }
}

void 
SerialDevice::handleExtendedReadOperation(const SplitWord32& addr, SerialDeviceOperations value) noexcept {
    switch (value) {
        case SerialDeviceOperations::RW:
            genericIOHandler<true>(addr, performSerialRead_Fast, performSerialWrite_Fast);
            break;
        case SerialDeviceOperations::Flush:
            Serial.flush();
            genericIOHandler<true>(addr);
            break;
        case SerialDeviceOperations::Baud:
            readOnlyDynamicValue(addr, getBaudRate());
            break;
        default:
            genericIOHandler<true>(addr);
            break;
    }
}
void 
SerialDevice::handleExtendedWriteOperation(const SplitWord32& addr, SerialDeviceOperations value) noexcept {
    switch (value) {
        case SerialDeviceOperations::RW:
            genericIOHandler<false>(addr, performSerialRead_Fast, performSerialWrite_Fast);
            break;
        case SerialDeviceOperations::Flush:
            Serial.flush();
            genericIOHandler<false>(addr);
            break;
        default:
            genericIOHandler<false>(addr);
            break;
    }
}

bool
SerialDevice::begin() noexcept {
    Serial.begin(baud_);
    return true;
}

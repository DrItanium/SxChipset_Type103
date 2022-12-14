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
#include "Peripheral.h"
#include "SerialDevice.h"
uint16_t
performSerialRead() noexcept {
    return Serial.read();
}

void
performSerialWrite(uint16_t value) noexcept {
    Serial.write(static_cast<uint8_t>(value));
}

void 
SerialDevice::setBaudRate(uint32_t baudRate) noexcept { 
    if (baudRate != baud_) {
        baud_ = baudRate; 
        Serial.end();
        Serial.begin(baud_);
    }
}

bool
SerialDevice::init() noexcept {
    Serial.begin(baud_);
    return true;
}


uint16_t 
SerialDevice::extendedRead(const Channel0Value& m0) const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (getCurrentOpcode()) {
        case SerialDeviceOperations::RW:
            return performSerialRead();
        case SerialDeviceOperations::Flush:
            Serial.flush();
            return 0;
        case SerialDeviceOperations::Baud:
            return SplitWord32{baud_}.retrieveHalf(getOffset());
        default:
            return 0;
    }
}
void 
SerialDevice::extendedWrite(const Channel0Value& m0, uint16_t value) noexcept {
    // do nothing
    bool setBaud = false;
    switch (getCurrentOpcode()) {
        case SerialDeviceOperations::RW:
            performSerialWrite(value);
            break;
        case SerialDeviceOperations::Flush:
            Serial.flush();
            break;
        case SerialDeviceOperations::Baud:
            baudAssignTemporary_.assignHalf(getOffset(), value);
            // when we assign the upper half then we want to set the baud
            setBaud = (getOffset() & 0b1);
            break;
        default:
            break;
    }
    if (setBaud) {
        Serial.flush();
        setBaudRate(baudAssignTemporary_.getWholeValue());
        baudAssignTemporary_.clear();
    }
}

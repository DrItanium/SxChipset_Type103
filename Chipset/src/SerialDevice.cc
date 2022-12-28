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


void 
SerialDevice::setBaudRate(uint32_t baudRate) noexcept { 
    if (baudRate != baud_) {
        baud_ = baudRate; 
        Serial.end();
        Serial.begin(baud_);
    }
}

bool
SerialDevice::begin() noexcept {
    Serial.begin(baud_);
    return true;
}


uint16_t 
SerialDevice::extendedRead(const Channel0Value& m0) const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (getCurrentOpcode()) {
        case SerialDeviceOperations::RW:
            return Serial.read();
        case SerialDeviceOperations::Flush:
            Serial.flush();
            return 0;
        case SerialDeviceOperation::Baud:
            return SplitWord32{baud_}.retrieveWord(getOffset());
        default:
            return 0;
    }
}
void 
SerialDevice::extendedWrite(const Channel0Value& m0, uint16_t value) noexcept {
    // do nothing
    switch (getCurrentOpcode()) {
        case SerialDeviceOperations::RW:
            Serial.write(static_cast<uint8_t>(value));
            break;
        case SerialDeviceOperations::Flush:
            Serial.flush();
            break;
        default:
            break;
    }
    /// @todo update write operations so that we cache results and perform the
    /// writes at the end
}

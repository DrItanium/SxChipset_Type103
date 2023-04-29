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

#ifndef CHIPSET_SERIALDEVICE_H
#define CHIPSET_SERIALDEVICE_H
#include "Types.h"
#include "Peripheral.h"
BeginDeviceOperationsList(SerialDevice)
    RW,
    Flush,
    Baud,
EndDeviceOperationsList(SerialDevice)

ConnectPeripheral(TargetPeripheral::Serial, SerialDeviceOperations);

class SerialDevice {
public:
    bool begin() noexcept;
    bool isAvailable() const noexcept { return true; }
    void setBaudRate(uint32_t baudRate) noexcept;
    [[nodiscard]] constexpr auto getBaudRate() const noexcept { return baud_; }
    inline void handleWriteOperations(const SplitWord128& body, uint8_t function, uint8_t offset) noexcept {
        using K = ConnectedOpcode_t<TargetPeripheral::Serial>;
        switch (getFunctionCode<TargetPeripheral::Serial>(function)) {
            case K::RW:
                Serial.write(static_cast<uint8_t>(body.bytes[0]));
                break;
            case K::Flush:
                Serial.flush();
                break;
            case K::Baud:
                setBaudRate(body[0].full);
                break;
            default:
                break;
        }
    }
private:
    uint32_t baud_ = 115200;
};
#endif //CHIPSET_SERIALDEVICE_H

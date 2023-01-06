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

#ifndef CHIPSET_INFODEVICE_H
#define CHIPSET_INFODEVICE_H
#include "Detect.h"
#include "Types.h"
#include "Peripheral.h"
BeginDeviceOperationsList(InfoDevice)
    GetChipsetClock,
    GetCPUClock,
EndDeviceOperationsList(InfoDevice)

class InfoDevice : public OperatorPeripheral<InfoDeviceOperations, InfoDevice> {
public:
    bool init() noexcept  { return true; }
    bool isAvailable() const noexcept { return true; }
    uint16_t extendedRead(const Channel0Value& m0) const noexcept;
    void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept;
    void onEndTransaction() noexcept { }
    void onStartTransaction(const SplitWord32&) noexcept { }
};

#endif //CHIPSET_INFODEVICE_H

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

#ifndef CHIPSET_INTERNALSTORAGEIO_H
#define CHIPSET_INTERNALSTORAGEIO_H
#include <Arduino.h>
#include "Detect.h"
#include "Types.h"
#include "Pinout.h"

/**
 * @brief Exposes banked memory to the i960 within io space; bypasses the cache, allows up to 16 megabytes to be mapped here
 */
class MemoryHandler final {
public:
    void begin(byte baseOffset) noexcept;
    void startTransaction(const SplitWord32&) noexcept;
    uint16_t read(const Channel0Value&) const noexcept;
    void write(const Channel0Value&, uint16_t ) noexcept;
    void endTransaction() noexcept;
    void next() noexcept;
private:
    SplitWord32 baseAddress_{0};
    byte baseBankOffset_ = 0;
    bool validMemory_ = false;
};

#endif //CHIPSET_INTERNALSTORAGEIO_H

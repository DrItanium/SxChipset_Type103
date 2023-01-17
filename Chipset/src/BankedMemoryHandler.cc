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

#include "InternalStorageIO.h"
#include "xmem.h"

void
MemoryHandler::startTransaction(const SplitWord32 & baseAddress) noexcept {
    baseAddress_ = baseAddress;
    auto theIndex = baseAddress_.onBoardMemoryAddress.bank + baseBankOffset_;
    xmem::setMemoryBank(theIndex);
    validMemory_ = xmem::validBank(theIndex);
    Serial.print(F("Bank Index: 0x"));
    Serial.println(theIndex, HEX);
}

void
MemoryHandler::endTransaction() noexcept {
    // nothing to really do
}

void
MemoryHandler::next() noexcept {
    if (validMemory_) {
        // next 16-bit word
        baseAddress_.full += 2;
    }
}

uint16_t
MemoryHandler::read(const Channel0Value & flags) const noexcept {
    if (validMemory_) {
        return adjustedMemory<uint16_t>(baseAddress_.onBoardMemoryAddress.offset);
    } else {
        return 0;
    }

}

void
MemoryHandler::write(const Channel0Value & flags, uint16_t value) noexcept {
    if (validMemory_) {
        switch (flags.getByteEnable()) {
            case EnableStyle::Full16:
                adjustedMemory<uint16_t>(baseAddress_.onBoardMemoryAddress.offset) = value;
                break;
            case EnableStyle::Lower8:
                adjustedMemory<uint8_t>(baseAddress_.onBoardMemoryAddress.offset) = static_cast<uint8_t>(value);
                break;
            case EnableStyle::Upper8:
                adjustedMemory<uint8_t>(baseAddress_.onBoardMemoryAddress.offset + 1) = static_cast<uint8_t>(value >> 8);
                break;
            default:
                break;
        }
    }
}

void
MemoryHandler::begin(byte baseOffset) noexcept {
    baseBankOffset_ = baseOffset;
    Serial.println(F("Banked Memory Exposure Enabled!"));
}
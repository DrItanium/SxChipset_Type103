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

#include "AddressTranslation.h"


SplitWord32
AddressTranslationDevice::translate(const SplitWord32& address) noexcept {
    if (active()) {
        /// @todo implement
        // carve the virtual address up into multiple parts and walk through
        // the table (and directory) if we have them. 
        return address;
    } else {
        // no virtual mapping look up actions
        return address;
    }
}



bool
AddressTranslationDevice::begin() noexcept {
    return true;
}

void
AddressTranslationDevice::enable() noexcept {
}

void
AddressTranslationDevice::disable() noexcept {

}
uint16_t
AddressTranslationDevice::extendedRead(const Channel0Value& m0) const noexcept {
    /// @todo implement support for caching the target info field so we don't
    /// need to keep looking up the dispatch address
    switch (getCurrentOpcode()) {
        case AddressTranslationDeviceOperations::Active:
            return active() ? 0xFFFF : 0x0000;
        default:
            return 0;
    }
}
void
AddressTranslationDevice::extendedWrite(const Channel0Value& m0, uint16_t value) noexcept {
    switch (getCurrentOpcode()) {
        case AddressTranslationDeviceOperations::Active:
            if (value) {
                enable();
            } else {
                disable();
            }
            break;
        default:
            break;
    }
}

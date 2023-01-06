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


#ifndef SXCHIPSET_ADDRESS_TRANSLATION_H__
#define SXCHIPSET_ADDRESS_TRANSLATION_H__
#include "Detect.h"
#include "Types.h"
#include "Peripheral.h"

BeginDeviceOperationsList(AddressTranslationDevice)
/**
 * @brief Reading from this register will return if the translator is active or not; Writes will activate/deactivate it
 */
    Active,
EndDeviceOperationsList(AddressTranslationDevice)
/**
 * @brief Converts virtual addresses to physical addresses; It follows the i960
 * region mapping concept where we divide the 32-bit memory space up into four
 * regions (0-3) with regions 0-2 being per process and the third region being
 * shared across all processes. This is where we can share data structures
 * across all targets. Region three is also where IO space is found as well.
 * This is mostly intentional but wasn't made with regions in mind. Each region
 * has its own TLB (address lookup cache) and is just another cache with the
 * same replacement algorithms as the data cache itself.
 */
class AddressTranslationDevice : public OperatorPeripheral<AddressTranslationDeviceOperations, AddressTranslationDevice> {
public:
    using Parent = OperatorPeripheral<AddressTranslationDeviceOperations, AddressTranslationDevice>;
    SplitWord32 translate(const SplitWord32& address) noexcept;
    void enable() noexcept;
    void disable() noexcept;
    [[nodiscard]] constexpr bool active() const noexcept { return enabled_; }
    bool isAvailable() const noexcept { return true; }
    bool begin() noexcept ;
    //void startTransaction(const SplitWord32& addr) noexcept override;
    uint16_t extendedRead(const Channel0Value& m0) const noexcept ;
    void extendedWrite(const Channel0Value& m0, uint16_t value) noexcept;
private:
    bool enabled_ = false;
};

#endif // end !defined(SXCHIPSET_ADDRESS_TRANSLATION_H__)

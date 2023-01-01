/**
* i960SxChipset_Type103
* Copyright (c) 2022-2023, Joshua Scoggins
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SXCHIPSET_TYPE103_TYPES_H__ 
#define SXCHIPSET_TYPE103_TYPES_H__ 
#include <stdint.h>
#include <Arduino.h>
union SplitWord32 {
    constexpr SplitWord32 (uint32_t a) : whole(a) { }
    uint32_t whole;
    uint8_t bytes[sizeof(uint32_t)/sizeof(uint8_t)];
    struct {
        uint32_t lower : 15;
        uint32_t a15 : 1;
        uint32_t a16_23 : 8;
        uint32_t a24_31 : 8;
    } splitAddress;
    struct {
        uint32_t lower : 15;
        uint32_t bank0 : 1;
        uint32_t bank1 : 1;
        uint32_t bank2 : 1;
        uint32_t bank3 : 1;
        uint32_t rest : 13;
    } internalBankAddress;
    struct {
        uint32_t offest : 28;
        uint32_t space : 4;
    } addressKind;
    struct {
        uint32_t offset : 23;
        uint32_t targetDevice : 3;
        uint32_t rest : 6;
    } psramAddress;
    [[nodiscard]] constexpr bool inIOSpace() const noexcept { return addressKind.space == 0b1111; }
    constexpr bool operator==(const SplitWord32& other) const noexcept {
        return other.whole == whole;
    }
    constexpr bool operator!=(const SplitWord32& other) const noexcept {
        return other.whole != whole;
    }
};
static constexpr auto pow2(uint8_t value) noexcept {
    return _BV(value);
}
static_assert(pow2(6) == 64);
static_assert(pow2(7) == 128);
static_assert(pow2(8) == 256);
#endif // !defined(SXCHIPSET_TYPE103_TYPES_H__)


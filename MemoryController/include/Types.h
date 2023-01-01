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

template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];
union SplitWord16 {
    uint16_t full;
    ElementContainer<uint16_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint16_t, uint8_t>; }
    constexpr SplitWord16() : full(0) { }
    constexpr explicit SplitWord16(uint16_t value) : full(value) { }
    constexpr explicit SplitWord16(uint8_t a, uint8_t b) : bytes{a, b} { }
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return full; }
    void setWholeValue(uint16_t value) noexcept { full = value; }
    [[nodiscard]] constexpr bool operator==(const SplitWord16& other) const noexcept { return full == other.full; }
    [[nodiscard]] constexpr bool operator!=(const SplitWord16& other) const noexcept { return full != other.full; }
    [[nodiscard]] constexpr bool operator==(uint16_t other) const noexcept { return full == other; }
    [[nodiscard]] constexpr bool operator!=(uint16_t other) const noexcept { return full != other; }
    [[nodiscard]] constexpr bool operator<(const SplitWord16& other) const noexcept { return full < other.full; }
    [[nodiscard]] constexpr bool operator<=(const SplitWord16& other) const noexcept { return full <= other.full; }
    [[nodiscard]] constexpr bool operator>(const SplitWord16& other) const noexcept { return full > other.full; }
    [[nodiscard]] constexpr bool operator>=(const SplitWord16& other) const noexcept { return full >= other.full; }
};
union SplitWord32 {
    constexpr SplitWord32 (uint32_t a) : full(a) { }
    uint32_t full;
    ElementContainer<uint32_t, uint8_t> bytes;
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
        return other.full == full;
    }
    constexpr bool operator!=(const SplitWord32& other) const noexcept {
        return other.full != full;
    }
};
static constexpr auto pow2(uint8_t value) noexcept {
    return _BV(value);
}
static_assert(pow2(0) == 1);
static_assert(pow2(1) == 2);
static_assert(pow2(2) == 4);
static_assert(pow2(3) == 8);
static_assert(pow2(4) == 16);
static_assert(pow2(5) == 32);
static_assert(pow2(6) == 64);
static_assert(pow2(7) == 128);
static_assert(pow2(8) == 256);
static_assert(pow2(9) == 512);
static_assert(pow2(10) == 1024);
#endif // !defined(SXCHIPSET_TYPE103_TYPES_H__)


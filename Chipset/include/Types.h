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

#ifndef SXCHIPSET_TYPE103_TYPES_H__
#define SXCHIPSET_TYPE103_TYPES_H__
#include <Arduino.h>
#include "Detect.h"

using uint24_t = __uint24;

template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];
template<typename T>
struct TagDispatcher {
    using UnderlyingType = T;
};
template<typename T>
using TreatAs = TagDispatcher<T>;
using TreatAsOrdinal = TreatAs<uint32_t>;

union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    struct {
        uint32_t offset : 14;
        uint32_t bank : 18;
    } bankView14;
    constexpr explicit SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32() : SplitWord32(0) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    [[nodiscard]] constexpr auto getBankIndex() const noexcept {
#if 0
        // the problem is that we are spanning two bytes in the _middle_ of an
        // address... so we have to treat them separately and merge them
        // together later on. So far, this seems to be the most optimal
        // implementation
#ifndef __BUILTIN_AVR_INSERT_BITS
        uint8_t lower = static_cast<uint8_t>(bytes[1] >> 6) & 0b11;
        uint8_t upper = static_cast<uint8_t>(bytes[2] << 2) & 0b1111'1100;
        return lower | upper;
#else
        return __builtin_avr_insert_bits(0xffffff76, bytes[1], 
                __builtin_avr_insert_bits(0x543210ff, bytes[2], 0));
#endif
#else
        return bankView14.bank;
#endif
    }
    constexpr size_t alignedBankAddress() const noexcept {
        return 0x4000 | (halves[0] & 0x3FFC);
    }
    constexpr size_t unalignedBankAddress() const noexcept {
        return 0x4000 | (halves[0] & 0x3FFF);
    }
};
static_assert(sizeof(SplitWord32) == sizeof(uint32_t), "SplitWord32 must be the exact same size as a 32-bit unsigned int");



union [[gnu::packed]] CH351 {
    uint8_t registers[8];
    struct {
        uint32_t data;
        uint32_t direction;
    } view32;
    struct {
        uint16_t data[2];
        uint16_t direction[2];
    } view16;
    struct {
        uint8_t data[4];
        uint8_t direction[4];
    } view8;
    struct {
        uint32_t ebi : 14;
        uint32_t bank : 18;
        uint32_t direction;
    } bankSwitching;
    struct {
        struct {
            uint8_t hold : 1;
            uint8_t hlda : 1;
            uint8_t lock : 1;
            uint8_t fail : 1;
            uint8_t reset : 1;
            uint8_t cfg : 3;
            uint8_t freq : 3;
            uint8_t backOff : 1;
            uint8_t ready : 1;
            uint8_t nmi : 1;
            uint8_t unused : 2;
            uint8_t xint0 : 1;
            uint8_t xint1 : 1;
            uint8_t xint2 : 1;
            uint8_t xint3 : 1;
            uint8_t xint4 : 1;
            uint8_t xint5 : 1;
            uint8_t xint6 : 1;
            uint8_t xint7 : 1;
            uint8_t byteEnable : 4;
            uint8_t den : 1;
            uint8_t blast : 1;
            uint8_t wr : 1;
            uint8_t bankSelect : 1;
        } data, direction;
    } ctl;

};
static_assert(sizeof(CH351) == 8);


#endif //SXCHIPSET_TYPE103_TYPES_H__

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
using int24_t = __int24;

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
struct AccessFromIBUS final { };
struct AccessFromXBUS final { };
struct AccessFromInstruction final { };

union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    constexpr explicit SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32() : SplitWord32(0) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return full; }
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    [[nodiscard]] constexpr bool isIOInstruction() const noexcept { return bytes[3] >= 0xF0; }
    [[nodiscard]] constexpr uint8_t getIODeviceCode() const noexcept { return bytes[2]; }
    [[nodiscard]] constexpr uint8_t getIOFunctionCode() const noexcept { return bytes[1]; }
    [[nodiscard]] constexpr uint8_t getIOGroup() const noexcept { return bytes[3]; }
    [[nodiscard]] constexpr uint8_t getIOMinorCode() const noexcept { return bytes[0]; }
    template<typename E>
    [[nodiscard]] constexpr E getIODevice() const noexcept { return static_cast<E>(getIODeviceCode()); }
    template<typename E>
    [[nodiscard]] constexpr E getIOFunction() const noexcept { return static_cast<E>(getIOFunctionCode()); }
    [[nodiscard]] constexpr uint8_t getAddressOffset() const noexcept { 
#ifdef __BUILTIN_AVR_INSERT_BITS
        return __builtin_avr_insert_bits(0xfffff321, bytes[0], 0);
#else
        return (bytes[0] & 0b1110) >> 1; 
#endif
    }
    [[nodiscard]] constexpr auto retrieveHalf(byte offset) const noexcept { return halves[offset & 0b1]; }
    [[nodiscard]] constexpr uint8_t getIBUSBankIndex() const noexcept {
        // the problem is that we are spanning two bytes in the _middle_ of an
        // address... so we have to treat them separately and merge them
        // together later on. So far, this seems to be the most optimal
        // implementation
#ifndef __BUILTIN_AVR_INSERT_BITS
        uint8_t lower = static_cast<uint8_t>(bytes[1] >> 6) & 0b11;
        uint8_t upper = static_cast<uint8_t>(bytes[2] << 2) & 0b1111'1100;
        return lower + upper;
#else
        return __builtin_avr_insert_bits(0xffffff76, bytes[1], 
                __builtin_avr_insert_bits(0x543210ff, bytes[2], 0));
#endif
    }
    void assignHalf(byte offset, uint16_t value) noexcept { halves[offset & 0b1] = value; }
    void clear() noexcept { full = 0; }
    void setWholeValue(uint32_t value) noexcept {
        full = value;
    }
    constexpr size_t alignedBankAddress(AccessFromIBUS) const noexcept {
        return 0x4000 + (halves[0] & 0x3FFC);
    }
    constexpr size_t alignedBankAddress(AccessFromXBUS) const noexcept {
        return 0x8000 + (halves[0] & 0x7FFC);
    }
    constexpr size_t unalignedBankAddress(AccessFromIBUS) const noexcept {
        return 0x4000 + (halves[0] & 0x3FFF);
    }
    constexpr size_t unalignedBankAddress(AccessFromXBUS) const noexcept {
        return 0x8000 + (halves[0] & 0x7FFF);
    }
    constexpr size_t transactionAlignedBankAddress(AccessFromIBUS) const noexcept {
        return 0x4000 + (halves[0] & 0x3FF0);
    }
    constexpr size_t transactionAlignedBankAddress(AccessFromXBUS) const noexcept {
        return 0x8000 + (halves[0] & 0x7FF0);
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
    } ctl;
};

struct SplitWord128 {
    uint8_t bytes[16];
    SplitWord32& getWord(uint8_t index) noexcept { return reinterpret_cast<SplitWord32*>(bytes)[index]; }
    const SplitWord32& getWord(uint8_t index) const noexcept { return reinterpret_cast<const SplitWord32*>(bytes)[index]; }
    volatile SplitWord32& getWord(uint8_t index) volatile noexcept { return reinterpret_cast<volatile SplitWord32*>(bytes)[index]; }
    [[nodiscard]] SplitWord32& operator[](uint8_t index) noexcept { return getWord(index); }
    [[nodiscard]] volatile SplitWord32& operator[](uint8_t index) volatile noexcept { return getWord(index); }
    [[nodiscard]] const SplitWord32& operator[](uint8_t index) const noexcept { return getWord(index); }
};

[[gnu::address(0x2200)]] inline volatile CH351 AddressLinesInterface;
[[gnu::address(0x2208)]] inline volatile CH351 DataLinesInterface;
[[gnu::address(0x2210)]] inline volatile CH351 ControlSignals;
[[gnu::address(0x2218)]] inline volatile CH351 XBUSBankRegister;


#endif //SXCHIPSET_TYPE103_TYPES_H__

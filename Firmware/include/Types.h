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

#ifndef SXCHIPSET_TYPE103_TYPES_H
#define SXCHIPSET_TYPE103_TYPES_H
#include <Arduino.h>
#include <SdFat.h>

template<typename W, typename E>
constexpr auto ElementCount = sizeof(W) / sizeof(E);
template<typename W, typename T>
using ElementContainer = T[ElementCount<W, T>];
extern SdFat SD;
constexpr auto OffsetSize = 4; // 16-byte line
constexpr auto TagSize = 7; // 8192 bytes divided into 16-byte
                                   // lines with 4 lines per set
                                   // (4-way)
constexpr auto KeySize = 32 - (OffsetSize + TagSize);
union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    constexpr SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    struct {
        uint32_t a0 : 1;
        uint32_t offset : 3;
        uint32_t rest : 28;
    } address;
    struct {
        uint32_t offset : OffsetSize;
        uint32_t tag : TagSize; 
        uint32_t key : KeySize;
    } cacheAddress;
    struct {
        uint32_t offset : 8;
        uint32_t key : 24;
    } ioDeviceAddress;
};

union SplitWord16 {
    uint16_t full;
    ElementContainer<uint16_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint16_t, uint8_t>; }
    constexpr SplitWord16() : full(0) { }
    constexpr explicit SplitWord16(uint16_t value) : full(value) { }
    constexpr explicit SplitWord16(uint8_t a, uint8_t b) : bytes{a, b} { }
};

union Channel0Value {
    explicit Channel0Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t den : 1;
        uint8_t w_r_ : 1;
        uint8_t fail : 1;
        uint8_t unused : 1;
        uint8_t addrInt : 4;
    } bits;
    constexpr bool isReadOperation() const noexcept { return bits.w_r_ == 0; }
    constexpr bool isWriteOperation() const noexcept { return bits.w_r_ != 0; }
};
union Channel1Value {
    explicit Channel1Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t byteEnable : 2;
        uint8_t blast : 1;
        uint8_t xioint : 1;
        uint8_t dataInt : 2;
        uint8_t ramIO : 1;
        uint8_t unused : 1;
    } bits;
};

struct CacheLine {
    virtual ~CacheLine() = default;
    virtual void clear() noexcept = 0;
    virtual void reset(SplitWord32 newAddress) noexcept = 0;
    virtual bool matches(SplitWord32 other) const noexcept = 0;
    virtual uint16_t getWord(byte offset) const noexcept = 0;
    virtual void setWord(byte offset, uint16_t value, bool enableLower, bool enableUpper) noexcept = 0;
    virtual void begin() noexcept { }
};
struct Cache {
    virtual ~Cache() = default;
    virtual void clear() noexcept = 0;
    virtual CacheLine& find(SplitWord32 address) noexcept = 0;
    virtual void begin() noexcept = 0;
};

struct CacheSet {
    virtual ~CacheSet() = default;
    virtual CacheLine& find(SplitWord32 address) noexcept = 0;
    virtual void clear() noexcept = 0;
    virtual void begin() noexcept = 0;
};

struct SplitCache : public Cache {
    ~SplitCache() override = default;
    virtual Cache& findCache(uint8_t address) noexcept = 0;
    CacheLine& find(SplitWord32 address) noexcept override {
        return findCache(address.bytes[3]).find(address);
    }
};
SplitCache& getCache() noexcept;
struct LineSink final : public CacheLine {
    ~LineSink() override = default;
    void reset(SplitWord32) noexcept override { }
    void clear() noexcept override { }
    void begin() noexcept override { }
    bool matches(SplitWord32) const noexcept override { return true; }
    uint16_t getWord(byte) const noexcept override { return 0; }
    void setWord(byte, uint16_t, bool, bool) noexcept override { } 
};
using IOSink = LineSink;

struct IODevice : public CacheLine {
    IODevice(uint32_t address) noexcept : addr_(address) { }
    ~IODevice() override = default;
    void reset(SplitWord32 ) noexcept override { }
    void clear() noexcept override { }
    bool matches(SplitWord32 other) const noexcept override { return other.ioDeviceAddress.key == addr_.ioDeviceAddress.key; }
    private:
        SplitWord32 addr_;
};

#endif //SXCHIPSET_TYPE103_TYPES_H

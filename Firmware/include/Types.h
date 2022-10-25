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
enum class IOGroup : byte{
    /**
     * @brief Serial console related operations
     */
    Serial,
    /**
     * @brief First 32-bit port accessor
     */
    GPIOA,
    /**
     * @brief Second 32-bit port accessor
     */
    GPIOB,
    /**
     * @brief Operations relating to the second SPI bus that we have exposed
     */
    SPI2,
    Undefined,
};
static_assert(static_cast<byte>(IOGroup::Undefined) <= 16, "Too many IO groups defined!");
constexpr IOGroup getGroup(uint8_t value) noexcept {
    switch (static_cast<IOGroup>(value & 0b1111)) {
        case IOGroup::Serial:
        case IOGroup::GPIOA:
        case IOGroup::GPIOB:
        case IOGroup::SPI2:
            return static_cast<IOGroup>(value);
        default:
            return IOGroup::Undefined;
    }
}
enum class SerialGroupFunction : byte {
    RWFast,
    RWCompact,
    RWDMAStyle,
    Flush,
};
enum class GPIOFunction : byte {
    IODIR,
    IPOL,
    GPINTEN,
    DEFVAL,
    INTCON,
    IOCON,
    GPPU,
    INTF,
    INTCAP,
    GPIO,
    OLAT,
};
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
        uint32_t subfunction : 16;
        uint32_t function : 8;
        uint32_t group : 4;
        uint32_t req : 4;
    } ioRequestAddress;
    [[nodiscard]] constexpr bool isIOInstruction() const noexcept { return ioRequestAddress.req == 0xF; }
    [[nodiscard]] constexpr IOGroup getIOGroup() const noexcept { return getGroup(ioRequestAddress.group); }
    [[nodiscard]] constexpr uint8_t getIOFunctionCode() const noexcept { return ioRequestAddress.function; }
    template<typename E>
    [[nodiscard]] constexpr E getIOFunction() const noexcept { return static_cast<E>(getIOFunctionCode()); }
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
constexpr uint8_t makeByteEnable(uint8_t be0, uint8_t be1) noexcept {
    return be0 | be1 << 1;
}
static_assert(makeByteEnable(0,0) == 0b00);
static_assert(makeByteEnable(1,0) == 0b01);
static_assert(makeByteEnable(0,1) == 0b10);
static_assert(makeByteEnable(1,1) == 0b11);
union Channel1Value {
    explicit Channel1Value(uint8_t value) noexcept : value_(value) { }
    uint8_t value_;
    struct {
        uint8_t be0 : 1;
        uint8_t be1 : 1;
        uint8_t blast : 1;
        uint8_t xioint : 1;
        uint8_t dataInt : 2;
        uint8_t ramIO : 1;
        uint8_t unused : 1;
    } bits;
    constexpr uint8_t getByteEnable() const noexcept {
        return makeByteEnable(bits.be0, bits.be1);
    }
};

size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
struct DataCacheLine {
    static constexpr auto NumberOfWords = 8;
    static constexpr auto NumberOfDataBytes = sizeof(SplitWord16)*NumberOfWords;
    uint16_t getWord(byte offset) const noexcept {
        return words[offset & 0b111].full;
    }
    void clear() noexcept {
        metadata.reg = 0;
        for (int i = 0; i < NumberOfWords; ++i) {
            words[i].full = 0;
        }
    }
    bool matches(SplitWord32 other) const noexcept {
        return metadata.valid_ && (other.cacheAddress.key == metadata.key);
    }
    void reset(SplitWord32 newAddress) noexcept {
        if (metadata.valid_ && metadata.dirty_) {
            auto copy = newAddress;
            copy.cacheAddress.offset = 0;
            copy.cacheAddress.key = metadata.key;
            memoryWrite(copy, reinterpret_cast<byte*>(words), NumberOfDataBytes);
        }
        metadata.valid_ = true;
        metadata.dirty_ = false;
        metadata.key = newAddress.cacheAddress.key;
        auto copy2 = newAddress;
        copy2.cacheAddress.offset = 0;
        memoryRead(copy2, reinterpret_cast<byte*>(words), NumberOfDataBytes);
    }
    void setWord(byte offset, uint16_t value, bool enableLower, bool enableUpper) noexcept {
        if (enableLower) {
            words[offset & 0b111].bytes[0] = value;
            metadata.dirty_ = true;
        }
        if (enableUpper) {
            words[offset & 0b111].bytes[1] = value >> 8;
            metadata.dirty_ = true;
        }
    }
    union {
        uint32_t reg;
        struct {
            uint32_t key : KeySize;
            uint32_t valid_ : 1;
            uint32_t dirty_ : 1;
        };
    } metadata;
    static_assert(sizeof(metadata) == sizeof(uint32_t), "Too many flags specified for metadata");
    SplitWord16 words[NumberOfWords];
    void begin() noexcept { }
    uint8_t* data() noexcept { return reinterpret_cast<uint8_t*>(words); }
    [[nodiscard]] constexpr size_t numWords() const noexcept { return NumberOfWords; }
    [[nodiscard]] constexpr size_t numBytes() const noexcept { return NumberOfDataBytes; }


};
struct DataCacheSet {
    static constexpr auto NumberOfLines = 4;
    void begin() noexcept {
        for (int i = 0; i < NumberOfLines; ++i) {
            lines[i].begin();
        }
    }
    auto& find(SplitWord32 address) noexcept {
        for (int i = 0; i < NumberOfLines; ++i) {
            if (lines[i].matches(address)) {
                return lines[i];
            }
        }
        auto& target = lines[replacementIndex_];
        ++replacementIndex_;
        replacementIndex_ %= NumberOfLines;
        target.reset(address);
        return target;
    }
    void clear() noexcept {
        replacementIndex_ = 0;
        for (int i = 0; i < NumberOfLines; ++i) {
            lines[i].clear();
        }
    }
    private:
        DataCacheLine lines[NumberOfLines];
        byte replacementIndex_ = 0;
};
struct DataCache {
    void clear() noexcept {
        for (int i = 0; i < 128; ++i) {
            cache[i].clear();
        }
    }
    auto& find(SplitWord32 address) noexcept {
        return cache[address.cacheAddress.tag].find(address);
    }
    void begin() noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
    private:
        DataCacheSet cache[128];
};



DataCache& getCache() noexcept;
void setupCache() noexcept;

#endif //SXCHIPSET_TYPE103_TYPES_H

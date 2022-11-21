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
    Drive,
    Undefined,
};
static_assert(static_cast<byte>(IOGroup::Undefined) <= 16, "Too many IO groups defined!");
constexpr IOGroup getGroup(uint8_t value) noexcept {
    switch (static_cast<IOGroup>(value & 0b1111)) {
        case IOGroup::Serial:
        case IOGroup::GPIOA:
        case IOGroup::GPIOB:
        case IOGroup::Drive:
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
enum class EnableStyle : byte {
    Full16 = 0b00,
    Upper8 = 0b01,
    Lower8 = 0b10,
    Undefined = 0b11,
};

union SplitWord32 {
    uint32_t full;
    ElementContainer<uint32_t, uint16_t> halves;
    ElementContainer<uint32_t, uint8_t> bytes;
    [[nodiscard]] constexpr auto numHalves() const noexcept { return ElementCount<uint32_t, uint16_t>; }
    [[nodiscard]] constexpr auto numBytes() const noexcept { return ElementCount<uint32_t, uint8_t>; }
    [[nodiscard]] constexpr auto getWholeValue() const noexcept { return full; }
    constexpr SplitWord32(uint32_t value) : full(value) { }
    constexpr SplitWord32(uint16_t lower, uint16_t upper) : halves{lower, upper} { }
    constexpr SplitWord32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a, b, c, d} { }
    struct {
        uint32_t a0 : 1;
        uint32_t offset : (OffsetSize - 1);
        uint32_t rest : (32 - OffsetSize);
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
    struct {
        uint8_t lowest;
        uint8_t middle;
        uint8_t higher : 7;
        uint8_t key : 2;
        uint8_t unused : 7;
    } psram2Address;
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

union Word8 {
    explicit Word8(uint8_t value = 0) noexcept : value_(value) { }

    uint8_t value_;
    struct {
        uint8_t den : 1;
        uint8_t w_r_ : 1;
        uint8_t fail : 1;
        uint8_t unused : 1;
        uint8_t lowerAddr : 2;
        uint8_t upperAddr : 2;
    } channel0;
    struct {
        uint8_t be : 2;
        uint8_t blast : 1;
        uint8_t xioint : 1;
        uint8_t dataInt : 2;
        uint8_t ramIO : 1;
        uint8_t unused : 1;
    } channel1;
    struct {
        uint8_t valid_ : 1;
        uint8_t dirty_ : 1;
    } lineFlags;
    [[nodiscard]] constexpr bool isReadOperation() const noexcept { return channel0.w_r_ == 0; }
    [[nodiscard]] constexpr bool isWriteOperation() const noexcept { return channel0.w_r_ != 0; }
    [[nodiscard]] constexpr EnableStyle getByteEnable() const noexcept { return static_cast<EnableStyle>(channel1.be); }
    [[nodiscard]] constexpr auto lineIsValid() const noexcept { return lineFlags.valid_; }
    [[nodiscard]] constexpr auto lineIsDirty() const noexcept { return lineFlags.dirty_; }
    void clear() noexcept {
        value_ = 0;
    }
};
using Channel1Value = Word8;
using Channel0Value = Word8;

size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
struct DataCacheLine {
    static constexpr auto NumberOfWords = 8;
    static constexpr auto NumberOfDataBytes = sizeof(SplitWord16)*NumberOfWords;
    static constexpr uint8_t WordMask = 0b111;
    inline uint16_t getWord(byte offset) const noexcept {
        return words[offset & WordMask].full;
    }
    inline void clear() noexcept {
        key_ = 0;
        flags_.clear();
        for (int i = 0; i < NumberOfWords; ++i) {
            words[i].full = 0;
        }
    }
    inline bool matches(SplitWord32 other) const noexcept {
        return flags_.lineIsValid() && (other.cacheAddress.key == key_);
    }
    inline void reset(SplitWord32 newAddress) noexcept {
        newAddress.cacheAddress.offset = 0;
        if (flags_.lineIsValid() && flags_.lineIsDirty()) {
            auto copy = newAddress;
            copy.cacheAddress.key = key_;
            memoryWrite(copy, reinterpret_cast<byte*>(words), NumberOfDataBytes);
        }
        flags_.lineFlags.valid_ = true;
        flags_.lineFlags.dirty_ = false;
        key_ = newAddress.cacheAddress.key;
        memoryRead(newAddress, reinterpret_cast<byte*>(words), NumberOfDataBytes);
    }
    inline void setWord(byte offset, uint16_t value, EnableStyle style) noexcept {
        switch (style) {
            case EnableStyle::Full16:
                words[offset & WordMask].full = value;
                flags_.lineFlags.dirty_ = true;
                break;
            case EnableStyle::Lower8:
                words[offset & WordMask].bytes[0] = value;
                flags_.lineFlags.dirty_ = true;
                break;
            case EnableStyle::Upper8:
                words[offset & WordMask].bytes[1] = (value >> 8);
                flags_.lineFlags.dirty_ = true;
                break;
            default:
                break;
        }
    }
    void begin() noexcept { 
        clear();
    }
    private:
        uint32_t key_ :KeySize;
        Word8 flags_;
        SplitWord16 words[NumberOfWords];


};
struct DataCacheSet {
    static constexpr auto NumberOfLines = 5;
    inline void begin() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(SplitWord32 address) noexcept {
        for (auto& line : lines) {
            if (line.matches(address)) {
                return line;
            }
        }
        auto& target = lines[replacementIndex_];
        ++replacementIndex_;
        if (replacementIndex_ == NumberOfLines) {
            replacementIndex_ = 0;
        }
        target.reset(address);
        return target;
    }
    inline void clear() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
    private:
        DataCacheLine lines[NumberOfLines];
        byte replacementIndex_;
};
struct DataCache {
    static constexpr auto NumberOfSets = 128;
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[gnu::always_inline]] inline auto& find(SplitWord32 address) noexcept {
        return cache[address.cacheAddress.tag].find(address);
    }
    inline void begin() noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
    private:
        DataCacheSet cache[NumberOfSets];
};



DataCache& getCache() noexcept;
void setupCache() noexcept;

#endif //SXCHIPSET_TYPE103_TYPES_H

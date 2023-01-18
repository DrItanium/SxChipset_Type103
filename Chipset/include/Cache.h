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
//
// Created by jwscoggins on 1/6/23.
//

#ifndef CHIPSET_CACHE_H
#define CHIPSET_CACHE_H
#include "Detect.h"
#include "Types.h"
#include "xmem.h"
size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
enum class SetConfiguration : uint8_t {
    DirectMapped = 1,
    TwoWayRoundRobin = 2,
    ThreeWayRoundRobin = 3,
    FourWayRoundRobin = 4,
    FiveWayRoundRobin = 5,
    SixWayRoundRobin = 6,
    SevenWayRoundRobin = 7,
    EightWayRoundRobin = 8,
    NineWayRoundRobin = 9,
    TenWayRoundRobin = 10,
    ElevenWayRoundRobin = 11,
    TwelveWayRoundRobin = 12,
    ThirteenWayRoundRobin = 13,
    FourteenWayRoundRobin = 14,
    FifteenWayRoundRobin = 15,
    SixteenWayRoundRobin = 16,
    /**
     * @brief Two way most recently used algorithm; Seems speedy enough
     */
    TwoWayLRU = 0x80,
    /**
     * @brief Two way most recently used algorithm; Seems speedy enough
     */
    TwoWayMRU,
    /**
     * @brief 4-way tree PLRU algorithm which is relatively speedy
     */
    FourWayTreePLRU,
    FourWayBitPLRU,
    /**
     * @brief Implement Tree PLRU for an 8-way design, pretty slow
     */
    EightWayTreePLRU,
    EightWayBitPLRU,
    /**
     * @brief 4-way Random Replacement algorithm which uses a doom style random table to make it as speedy as possible; Relatively fast
     */
    RandomReplacement4,
    /**
     * @brief 4-way Random Replacement algorithm which uses the micros routine to choose the index to replace
     */
    RandomReplacement4_Micros,
    /**
     * @brief 8-way Random Replacement algorithm which uses a doom style random table to make it as speedy as possible; Relatively fast
     */
    RandomReplacement8,
    /**
     * @brief 8-way Random Replacement algorithm which uses the micros routine to choose the index to replace
     */
    RandomReplacement8_Micros,
    /**
     * @brief Update flags is called on a match and reset; If the index is same as the replacement target then increment the replacement target
     */
    ModifiedEightWayRoundRobin,
    /**
     * @brief The Direct Mapped implementation with a small 4 entry victim cache that is fully associative
     */
    DirectMappedWithVictimCache,
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration config>
union BasicCacheAddress {
    using Self = BasicCacheAddress<offsetBits, tagBits, bankBits, config>;
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto OffsetWordBitsCount = OffsetBitsCount - 1;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = bankBits;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount ;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    constexpr explicit BasicCacheAddress(uint32_t address) : backingStore_(address) { }
    constexpr explicit BasicCacheAddress(const SplitWord32& address) : backingStore_(address) { }
    constexpr auto getBankIndex() const noexcept { return bank; }
    constexpr auto getOffset() const noexcept { return offset; }
    constexpr auto getTag() const noexcept { return tag; }
    constexpr auto getKey() const noexcept { return key; }
    constexpr auto getBackingStore() const noexcept { return backingStore_; }
    void setOffset(uint32_t value) noexcept { offset = value; }
    bool matches(const Self& other) const noexcept { return compare.check == other.compare.check; }
    inline void clear() noexcept { backingStore_.clear(); }
    constexpr auto getWordOffset() const noexcept { return wordView.offset; }
private:
    SplitWord32 backingStore_;
    struct {
        uint32_t offset : OffsetBitsCount;
        uint32_t tag : TagBitsCount;
        uint32_t bank : BankBitsCount;
        uint32_t key : KeyBitsCount;
    };
    struct {
        uint32_t offset : OffsetBitsCount;
        uint32_t check : (TagBitsCount + KeyBitsCount + BankBitsCount);
    } compare;
    struct {
        uint32_t a0 : 1;
        uint32_t offset : OffsetWordBitsCount;
        uint32_t rest : (TagBitsCount + KeyBitsCount + BankBitsCount);
    } wordView;
};

template<uint8_t offsetBits, uint8_t tagBits, SetConfiguration config>
union BasicCacheAddress<offsetBits, tagBits, 0, config> {
    using Self = BasicCacheAddress<offsetBits, tagBits, 0, config>;
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto OffsetWordBitsCount = OffsetBitsCount - 1;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = 0;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount ;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    constexpr explicit BasicCacheAddress(uint32_t address) : backingStore_(address) { }
    constexpr explicit BasicCacheAddress(const SplitWord32& address) : backingStore_(address) { }
    constexpr auto getBankIndex() const noexcept { return 0; }
    constexpr auto getOffset() const noexcept { return offset; }
    constexpr auto getTag() const noexcept { return tag; }
    constexpr auto getKey() const noexcept { return key; }
    constexpr auto getBackingStore() const noexcept { return backingStore_; }
    constexpr auto getWordOffset() const noexcept { return wordView.offset; }
    void setOffset(uint32_t value) noexcept { offset = value; }
    bool matches(const Self& other) const noexcept { return compare.check == other.compare.check; }
    inline void clear() noexcept { backingStore_.clear(); }
private:
    SplitWord32 backingStore_;
    struct {
        uint32_t offset : OffsetBitsCount;
        uint32_t tag : TagBitsCount;
        uint32_t key : KeyBitsCount;
    };
    struct {
       uint32_t offset : OffsetBitsCount;
       uint32_t check : (TagBitsCount + KeyBitsCount);
    } compare;
    struct {
        uint32_t a0 : 1;
        uint32_t offset : OffsetWordBitsCount;
        uint32_t rest : (TagBitsCount + KeyBitsCount);
    } wordView;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration config>
struct BasicDataCacheLine {
    using CacheAddress = BasicCacheAddress<offsetBits, tagBits, bankBits, config>;
    static constexpr auto NumberOfDataBytes = pow2(offsetBits);
    static constexpr auto NumberOfWords = NumberOfDataBytes / sizeof(SplitWord16);
    inline void clear() noexcept {
        dest_.clear();
        flags_.reg = 0;
        for (auto& word : words) {
            word.full = 0;
        }
    }
    inline bool matches(const CacheAddress& other) const noexcept {
        return flags_.valid_ && (other.getKey() == dest_.getKey());
    }
    inline void sync() noexcept {
        if (flags_.valid_ && flags_.dirty_) {
            memoryWrite(dest_.getBackingStore(), reinterpret_cast<uint8_t*>(words), NumberOfDataBytes);
            flags_.dirty_ = false;
        }
    }
    inline void reset(const CacheAddress& newAddress) noexcept {
        sync();
        flags_.valid_ = true;
        flags_.dirty_ = false; // mark it implicitly as dirty
        dest_ = newAddress;
        dest_.setOffset(0);
        memoryRead(dest_.getBackingStore(), reinterpret_cast<uint8_t *>(words), NumberOfDataBytes);
    }
    void begin() noexcept {
        clear();
    }
    [[nodiscard]] const SplitWord16* getData() const noexcept {
        return words;
    }
    [[nodiscard]] inline uint16_t getWord(byte offset) const noexcept {
        return words[offset].getWholeValue();
    }
    inline void setWord(byte offset, uint16_t value, EnableStyle style) noexcept {
        flags_.dirty_ = true;
        switch (style) {
            case EnableStyle::Full16:
                words[offset].full = value;
                break;
            case EnableStyle::Lower8:
                words[offset].bytes[0] = static_cast<uint8_t>(value);
                break;
            case EnableStyle::Upper8:
                words[offset].bytes[1] = static_cast<uint8_t>(value >> 8);
                break;
            default:
                break;
        }
    }
    inline bool victimCacheMatches(const CacheAddress& address) const noexcept {
        return flags_.valid_ && dest_.matches(address);
    }
private:
    CacheAddress dest_{0};
    struct {
        uint8_t reg;
        struct {
            uint8_t dirty_ : 1;
            uint8_t valid_ : 1;
        };
    } flags_;
    SplitWord16 words[NumberOfWords];
};
constexpr bool requiresCustomImplementation(SetConfiguration cfg) noexcept {
    auto val = static_cast<uint8_t>(cfg);
    return val < 2 || val > 16;
}
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration cfg>
struct BasicDataCacheSet {
    static_assert(!requiresCustomImplementation(cfg), "A custom implementation must be defined for this configuration!");
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, cfg>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = static_cast<uint8_t>(cfg);
    inline void begin() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[replacementIndex_];
        updateFlags();
        target.reset(address);
        return target;
    }
    inline void updateFlags() noexcept {
        ++replacementIndex_;
        if (replacementIndex_ == NumberOfLines) {
            replacementIndex_ = 0;
        }
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


template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::ModifiedEightWayRoundRobin> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::ModifiedEightWayRoundRobin>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = static_cast<uint8_t>(8);
    inline void begin() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                updateFlags(i);
                return line;
            }
        }
        auto index = replacementIndex_;
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    inline void updateFlags(byte index) noexcept {
        if (replacementIndex_ == index) {
            ++replacementIndex_;
        }
    }
    inline void clear() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    byte replacementIndex_ : 3;
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::TwoWayLRU> {
    // use LRU instead of round robin
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::TwoWayLRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 2;
    inline void begin() noexcept {
        replacementTarget_ = false;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    [[nodiscard]] inline byte getTargetLine() const noexcept {
        return replacementTarget_ ? 1 : 0;
    }
    inline void updateFlags(byte index) noexcept {
        replacementTarget_ = index != 1;
    }
    inline void clear() noexcept {
        replacementTarget_ = false;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    bool replacementTarget_;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::TwoWayMRU> {
    // use MRU instead of round robin
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::TwoWayMRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 2;
    inline void begin() noexcept {
        replacementTarget_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        for (int i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    [[nodiscard]] inline byte getTargetLine() const noexcept {
        return replacementTarget_;
    }
    inline void updateFlags(int index) noexcept {
        replacementTarget_ = index;
    }
    inline void clear() noexcept {
        replacementTarget_ = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    byte replacementTarget_ : 1;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::FourWayTreePLRU> {
    // use MRU instead of round robin
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::FourWayTreePLRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 4;
    // translate from a bit pattern to a target address to access and then choose the opposite of
    // the target
    static constexpr byte TranslationTable[8] {
        0, // 0b0 x 0 (go left and choose left)
        1, // 0b0 x 1 (go right and choose right)
        0, // 0b0 x 0 (go left and choose left)
        1, // 0b0 x 1 (go right and choose right)
        2, // 0b1 0 x (go right and choose left)
        2, // 0b1 0 x (go right and choose left )
        3, // 0b1 1 x (go right and choose right)
        3, // 0b1 1 x (go right and choose right)
    };
    inline void begin() noexcept {
        flags_.reg = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(CacheAddress address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    inline uint8_t getTargetLine() const noexcept {
        return TranslationTable[flags_.reg];
    }
    inline void updateFlags(uint8_t index) noexcept {
        // set the node flags to denote the direction that is opposite to the direction taken
        switch (index & 0b11) {
            case 0: // 0b00
                flags_.top = 1;
                flags_.left = 1;
                break;
            case 1: // 0b01
                flags_.top = 1;
                flags_.left = 0;
                break;
            case 2: // 0b10
                flags_.top = 0;
                flags_.right = 1;
                break;
            case 3: // 0b11
                flags_.top = 0;
                flags_.right = 0;
                break;
        }
    }
    inline void clear() noexcept {
        flags_.reg = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    union {
        uint8_t reg;
        struct {
            uint8_t left : 1;
            uint8_t right : 1;
            uint8_t top : 1;
        };
    } flags_;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::FourWayBitPLRU> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::FourWayBitPLRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 4;
    inline void begin() noexcept {
        bits_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    inline uint8_t getTargetLine() const noexcept {
        // return the left most line whose flag bit is zero
        static constexpr uint8_t IndexTable[16] {
            3, // 0b0000
            3, // 0b0001
            3, // 0b0010
            3, // 0b0011
            3, // 0b0100
            3, // 0b0101
            3, // 0b0110
            3, // 0b0111
            2, // 0b1000
            2, // 0b1001
            2, // 0b1010
            2, // 0b1011
            1, // 0b1100
            1, // 0b1101
            0, // 0b1110
            0, // 0b1111
        };
        return IndexTable[bits_];
    }
    inline void updateFlags(uint8_t index) noexcept {
        // set the node flags to denote the direction that is opposite to the direction taken
        static constexpr uint8_t MaskTable[] {
                _BV(0),
                _BV(1),
                _BV(2),
                _BV(3),
        };
        auto targetBits = MaskTable[index & 0b11];
        bits_ |= targetBits;
        if (bits_ == 0b1111) {
            bits_ = targetBits;
        }
    }
    inline void clear() noexcept {
        bits_ = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    uint8_t bits_ : 4;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::EightWayBitPLRU> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::EightWayBitPLRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 8;
    inline void begin() noexcept {
        bits_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    static constexpr byte computeTargetBit(uint8_t value) noexcept {
        if (value & _BV(7)) {
            if (value & _BV(6)) {
                if (value & _BV(5)) {
                    if (value & _BV(4))  {
                        if (value & _BV(3))  {
                            if (value & _BV(2)) {
                                if (value & _BV(1)) {
                                    return 0;
                                } else {
                                    return 1;
                                }
                            }  else {
                                return 2;
                            }
                        } else {
                            return 3;
                        }
                    } else {
                        return 4;
                    }
                } else {
                    return 5;
                }
            } else {
                return 6;
            }
        } else {
            return 7;
        }
    }
    inline uint8_t getTargetLine() const noexcept {
        // return the left most line whose flag bit is zero
        static constexpr uint8_t IndexTable[256] {
#define X(base) \
                computeTargetBit((16 * base) + 0 ),  \
                computeTargetBit((16 * base) + 1 ), \
                computeTargetBit((16 * base) + 2 ), \
                computeTargetBit((16 * base) + 3 ), \
                computeTargetBit((16 * base) + 4 ), \
                computeTargetBit((16 * base) + 5 ), \
                computeTargetBit((16 * base) + 6 ), \
                computeTargetBit((16 * base) + 7 ), \
                computeTargetBit((16 * base) + 8 ),  \
                computeTargetBit((16 * base) + 9 ), \
                computeTargetBit((16 * base) + 10 ), \
                computeTargetBit((16 * base) + 11 ), \
                computeTargetBit((16 * base) + 12 ), \
                computeTargetBit((16 * base) + 13 ), \
                computeTargetBit((16 * base) + 14 ), \
                computeTargetBit((16 * base) + 15 )
            X(0),
            X(1),
            X(2),
            X(3),
            X(4),
            X(5),
            X(6),
            X(7),
            X(8),
            X(9),
            X(10),
            X(11),
            X(12),
            X(13),
            X(14),
            X(15),
#undef X
        };
        static_assert(IndexTable[0xFF] == 0);
        static_assert(IndexTable[0xFE] == 0);
        static_assert(IndexTable[0xFD] == 1);
        return IndexTable[bits_];
    }
    inline void updateFlags(uint8_t index) noexcept {
        // set the node flags to denote the direction that is opposite to the direction taken
        static constexpr uint8_t MaskTable[] {
                _BV(0),
                _BV(1),
                _BV(2),
                _BV(3),
                _BV(4),
                _BV(5),
                _BV(6),
                _BV(7),
        };
        auto targetBits = MaskTable[index & 0b111];
        bits_ |= targetBits;
        if (bits_ == 0b1111'1111) {
            bits_ = targetBits;
        }
    }
    inline void clear() noexcept {
        bits_ = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    uint8_t bits_;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::EightWayTreePLRU> {
    // use MRU instead of round robin
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::EightWayTreePLRU>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 8;
    // translate from a bit pattern to a target address to access and then choose the opposite of
    // the target
    static constexpr byte TranslationTableReduced[8] {
            0, // 0b0 x 0 (go left and choose left)
            1, // 0b0 x 1 (go right and choose right)
            0, // 0b0 x 0 (go left and choose left)
            1, // 0b0 x 1 (go right and choose right)
            2, // 0b1 0 x (go right and choose left)
            2, // 0b1 0 x (go right and choose left )
            3, // 0b1 1 x (go right and choose right)
            3, // 0b1 1 x (go right and choose right)
    };
    static constexpr byte TranslationTableReduced2[8] {
            4, // 0b0 x 0 (go left and choose left)
            5, // 0b0 x 1 (go right and choose right)
            4, // 0b0 x 0 (go left and choose left)
            5, // 0b0 x 1 (go right and choose right)
            6, // 0b1 0 x (go right and choose left)
            6, // 0b1 0 x (go right and choose left )
            7, // 0b1 1 x (go right and choose right)
            7, // 0b1 1 x (go right and choose right)
    };
    inline void begin() noexcept {
        reg = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address);
        return target;
    }
    [[nodiscard]] constexpr inline uint8_t getTargetLine() const noexcept {
        if (start == 0) {
            return TranslationTableReduced[group0];
        } else {
            return TranslationTableReduced2[group1];
        }
    }
    inline void updateFlags(uint8_t index) noexcept {
        // set the node flags to denote the direction that is opposite to the direction taken
        switch (index & 0b111) {
            case 0: // 0b000
                top0 = 1;
                left0 = 1;
                start = 1;
                break;
            case 1: // 0b001
                top0 = 1;
                left0 = 0;
                start = 1;
                break;
            case 2: // 0b010
                top0 = 0;
                right0 = 1;
                start = 1;
                break;
            case 3: // 0b011
                top0 = 0;
                right0 = 0;
                start = 1;
                break;
            case 4: // 0b100
                top1 = 1;
                left1 = 1;
                start = 0;
                break;
            case 5: // 0b101
                top1 = 1;
                left1 = 0;
                start = 0;
                break;
            case 6: // 0b110
                top1 = 0;
                right1 = 1;
                start = 0;
                break;
            case 7: // 0b111
                top1 = 0;
                right1 = 0;
                start = 0;
                break;
        }
    }
    inline void clear() noexcept {
        reg = 0;
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
    union {
        uint8_t reg;
        struct {
            uint8_t left0 : 1;
            uint8_t right0 : 1;
            uint8_t top0 : 1;
            uint8_t left1 : 1;
            uint8_t right1 : 1;
            uint8_t top1 : 1;
            uint8_t start : 1;
        };
        struct {
            uint8_t group0 : 3;
            uint8_t group1 : 3;
        };
    } ;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement4> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement4>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 4;
private:
    static inline byte RandomTable[256] { 0 };
    [[nodiscard]] byte getReplacementIndex() noexcept {
        auto ind = replacementIndex_;
        ++replacementIndex_;
        return RandomTable[ind];
    }
public:
    inline void begin() noexcept {
        static bool cacheSetup = false;
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
        if (!cacheSetup) {
            cacheSetup = true;
            for (int i = 0; i < 256; ++i) {
                RandomTable[i] = random() % NumberOfLines;
            }
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[getReplacementIndex()];
        updateFlags();
        target.reset(address);
        return target;
    }
    inline void updateFlags() noexcept { }
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

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement4_Micros> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement4_Micros>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 4;
private:
    [[nodiscard]] byte getReplacementIndex() noexcept {
        return micros() & 0b11;
    }
public:
    inline void begin() noexcept {
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[getReplacementIndex()];
        target.reset(address);
        return target;
    }
    inline void clear() noexcept {
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement8> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement8>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 8;
private:
    static inline byte RandomTable[256] { 0 };
    [[nodiscard]] byte getReplacementIndex() noexcept {
        auto ind = replacementIndex_;
        ++replacementIndex_;
        return RandomTable[ind];
    }
public:
    inline void begin() noexcept {
        static bool cacheSetup = false;
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
        if (!cacheSetup) {
            cacheSetup = true;
            for (int i = 0; i < 256; ++i) {
                RandomTable[i] = random() % NumberOfLines;
            }
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[getReplacementIndex()];
        updateFlags();
        target.reset(address);
        return target;
    }
    inline void updateFlags() noexcept { }
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

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement8_Micros> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::RandomReplacement8_Micros>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 8;
private:
    [[nodiscard]] byte getReplacementIndex() noexcept {
        return micros() & 0b111;
    }
public:
    inline void begin() noexcept {
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(const CacheAddress& address) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[getReplacementIndex()];
        updateFlags();
        target.reset(address);
        return target;
    }
    inline void updateFlags() noexcept { }
    inline void clear() noexcept {
        for (auto& line : lines) {
            line.clear();
        }
    }
private:
    DataCacheLine lines[NumberOfLines];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, SetConfiguration::DirectMapped> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::DirectMapped>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 1;
    inline void begin() noexcept {
        line.begin();
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        if (!line.matches(address)){
            line.reset(address);
        }
        return line;
    }
    inline void clear() noexcept {
        line.clear();
    }
private:
    DataCacheLine line;
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration cfg>
struct BasicDataCache {
    using DataCacheSet = BasicDataCacheSet<offsetBits, tagBits, bankBits, cfg>;
    using DataCacheLine = typename DataCacheSet::DataCacheLine;
    using CacheAddress = typename DataCacheSet::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        return cache[address.getTag()].find(address);
    }
    inline void begin(byte = 0) noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
private:
    DataCacheSet cache[NumberOfSets];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCache<offsetBits, tagBits, bankBits, SetConfiguration::DirectMapped> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::DirectMapped>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        auto& line = cache[address.getTag()];
        if (!line.matches(address)){
            line.reset(address);
        }
        return line;
    }
    inline void begin(byte = 0) noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
private:
    DataCacheLine cache[NumberOfSets];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCache<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    static constexpr auto NumberOfVictimCacheEntries = 8;
    inline void clear() noexcept {
        for (auto& set : cache) {
            set->clear();
        }
        for (auto& set : victimCache) {
            set->clear();
        }
    }
    [[nodiscard]] inline auto& find(const CacheAddress& address) noexcept {
        auto* line = cache[address.getTag()];
        if (!line->matches(address)){
            // okay so we have a miss, look through the victim cache to see if we can find a match
            if (auto index = findInVictimCache(address); index != -1) {
                // okay so we found a match in the victim cache, do the swap and
                auto* newLine = installToVictimCache(line, index);
                cache[address.getTag()] = newLine; // install the new line into the cache proper
                return *newLine;
            } else {
                // okay so we have a double miss, get a line and swap it with this one
                auto* newLine = installToVictimCache(line);
                cache[address.getTag()] = newLine; // install the new line into the cache proper
                newLine->reset(address);
                return *newLine;
            }
            // do the reset as normal
        }
        return *line;
    }
    inline void begin(byte = 0) noexcept {
        for (int i = 0; i < NumberOfSets; ++i) {
            cache[i] = new DataCacheLine();
            cache[i]->begin();
        }
        for (int i  = 0; i < NumberOfVictimCacheEntries; ++i) {
            victimCache[i] = new DataCacheLine();
            victimCache[i]->begin();
        }
    }
private:
    /**
     * @brief See if the given cache address can be found in the victim cache via a linear scan
     * @param address The cache address to look for
     * @return The index in the victim cache of the line (-1 if not found)
     */
    [[nodiscard]] int findInVictimCache(const CacheAddress& address) noexcept {
        for (int i = 0; i < NumberOfVictimCacheEntries; ++i) {
            if (auto* line = victimCache[i]; line->victimCacheMatches(address)) {
                return i;
            }
        }
        return -1;
    }
    /**
     * @brief Installs the given line to the victim cache at the specified address (found from doing a linear search)
     * @param line The line to install
     * @param index The position to install to
     * @return The old cache line found at that position (note, it could be an invalid cache line or have data in it!)
     */
    [[nodiscard]] DataCacheLine* installToVictimCache(DataCacheLine* line, int index) noexcept {
        auto* current = victimCache[index];
        victimCache[index] = line;
        return current;
    }

    /**
     * @brief Use an internal replacement counter to select a line from the victim cache and do the normal swap.
     * This version is far more useful when you have encountered a double miss and need to do the replacement.
     * The selection algorithm is round robin.
     * @param line The line to be saved into the victim cache
     * @return The old cache line
     */
    [[nodiscard]] DataCacheLine* installToVictimCache(DataCacheLine* line) noexcept {
        auto* result = installToVictimCache(line, currentReplacementIndex_);
        ++currentReplacementIndex_;
        if (currentReplacementIndex_ == NumberOfVictimCacheEntries) {
            currentReplacementIndex_ = 0;
        }
        return result;
    }
private:
    DataCacheLine* cache[NumberOfSets];
    DataCacheLine* victimCache[NumberOfVictimCacheEntries];
    uint8_t currentReplacementIndex_ = 0;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration cfg>
struct BasicCacheReference {
    using Cache = BasicDataCache<offsetBits, tagBits, bankBits, cfg>;
    using DataCacheLine = typename Cache::DataCacheLine;
    using CacheAddress = typename Cache::CacheAddress;
    void select() const noexcept {
        xmem::setMemoryBank(index_);
    }
    void begin(byte index) noexcept {
        index_ = index;
        select();
        ptr_ = new Cache();
        ptr_->begin();
    }
    auto& find(const CacheAddress& addr) noexcept {
        // make sure we select this bank before we jump to the pointer
        select();
        return ptr_->find(addr);
    }
    void clear() noexcept {
        select();
        ptr_->clear();
    }
private:
    byte index_ = 0;
    Cache* ptr_ = nullptr;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, SetConfiguration cfg>
struct CachePool {
    using CacheReference = BasicCacheReference<offsetBits, tagBits, bankBits, cfg>;
    using CacheAddress = typename CacheReference::CacheAddress;
    using CacheLine = typename CacheReference::DataCacheLine;
    static constexpr auto NumberOfBanks = pow2(bankBits);
    void begin(byte bankOffset) noexcept {
        baseIndex_ = bankOffset;
        for (int i = 0, j = bankOffset; i < NumberOfBanks; ++i, ++j) {
            pool_[i].begin(j);
        }
    }
    inline auto& find(const SplitWord32& address) noexcept {
        CacheAddress addr(address);
        return pool_[addr.getBankIndex()].find(addr);
    }
    void clear()  {
        for (auto& bank : pool_) {
            bank.clear();
        }
    }
private:
    byte baseIndex_ = 0;
    CacheReference pool_[NumberOfBanks];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct CachePool<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache> {
    using Cache = BasicDataCache<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache>;
    //using CacheReference = BasicCacheReference<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache>;
    using CacheAddress = typename Cache::CacheAddress;
    using CacheLine = typename Cache::DataCacheLine;
    static constexpr auto NumberOfBanks = pow2(bankBits);
    void begin(byte bankOffset) noexcept {
        baseIndex_ = bankOffset;
        for (int i = 0; i < NumberOfBanks; ++i) {
            indicies_[i] = i + baseIndex_;
            xmem::setMemoryBank(indicies_[i]);
            pool_[i] = new Cache();
            pool_[i]->begin(0);
        }
    }
    inline auto& find(const SplitWord32& address) noexcept {
        CacheAddress addr(address);
        xmem::setMemoryBank(indicies_[addr.getBankIndex()]);
        return pool_[addr.getBankIndex()]->find(addr);
    }
    void clear()  {
        for (auto* bank : pool_) {
            bank->clear();
        }
    }
private:
    byte baseIndex_ = 0;
    byte indicies_[NumberOfBanks];
    Cache* pool_[NumberOfBanks];
};

template<uint8_t offsetBits, uint8_t tagBits>
struct CachePool<offsetBits, tagBits, 0, SetConfiguration::DirectMappedWithVictimCache> {
    using Cache = BasicDataCache<offsetBits, tagBits, 0, SetConfiguration::DirectMappedWithVictimCache>;
    //using CacheReference = BasicCacheReference<offsetBits, tagBits, bankBits, SetConfiguration::DirectMappedWithVictimCache>;
    using CacheAddress = typename Cache::CacheAddress;
    using CacheLine = typename Cache::DataCacheLine;
    static constexpr auto NumberOfBanks = pow2(0);
    void begin(byte bankOffset) noexcept {
        baseIndex_ = bankOffset;
        xmem::setMemoryBank(0);
        pool_ = new Cache();
        pool_->begin(0);
    }
    inline auto& find(const SplitWord32& address) noexcept {
        CacheAddress addr(address);
        xmem::setMemoryBank(0);
        return pool_->find(addr);
    }
    void clear()  {
        pool_->clear();
    }
private:
    byte baseIndex_ = 0;
    Cache* pool_ = nullptr;
};

constexpr auto NumberOfBankBits = 0;
constexpr auto NumberOfOffsetBits = 6;
constexpr auto NumberOfTagBits = 7;
constexpr auto OffChipSetConfiguration = SetConfiguration::DirectMappedWithVictimCache;
using OffChipMemoryCache = CachePool<NumberOfOffsetBits, NumberOfTagBits, NumberOfBankBits, OffChipSetConfiguration>;
constexpr auto OnChipOffsetBits = 4;
constexpr auto OnChipTagBits = 7;
constexpr auto OnChipSetConfiguration = SetConfiguration::TwoWayLRU;
using OnChipMemoryCache = BasicDataCache<OnChipOffsetBits, OnChipTagBits, 0, OnChipSetConfiguration>;
using MemoryCache = OffChipMemoryCache;

MemoryCache& getCache() noexcept;
void setupCache() noexcept;
#endif //CHIPSET_CACHE_H

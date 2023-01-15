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
constexpr bool EnableCacheDebugging = false;
size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
union BasicCacheAddress {
    using Self = BasicCacheAddress<offsetBits, tagBits, bankBits>;
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto OffsetWordBitsCount = OffsetBitsCount - 1;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = bankBits;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount ;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    BasicCacheAddress(uint32_t address) : backingStore_(address) { }
    BasicCacheAddress(const SplitWord32& address) : backingStore_(address) { }
    constexpr auto getBankIndex() const noexcept { return bank; }
    constexpr auto getOffset() const noexcept { return offset; }
    constexpr auto getTag() const noexcept { return tag; }
    constexpr auto getKey() const noexcept { return key; }
    constexpr auto getBackingStore() const noexcept { return backingStore_; }
    void setOffset(uint32_t value) noexcept { offset = value; }
    void setKey(uint32_t value) noexcept { key = value; }
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

template<uint8_t offsetBits, uint8_t tagBits>
union BasicCacheAddress<offsetBits, tagBits, 0> {
    using Self = BasicCacheAddress<offsetBits, tagBits, 0>;
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto OffsetWordBitsCount = OffsetBitsCount - 1;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = 0;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount ;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    BasicCacheAddress(uint32_t address) : backingStore_(address) { }
    BasicCacheAddress(const SplitWord32& address) : backingStore_(address) { }
    constexpr auto getBankIndex() const noexcept { return 0; }
    constexpr auto getOffset() const noexcept { return offset; }
    constexpr auto getTag() const noexcept { return tag; }
    constexpr auto getKey() const noexcept { return key; }
    constexpr auto getBackingStore() const noexcept { return backingStore_; }
    constexpr auto getWordOffset() const noexcept { return wordView.offset; }
    void setOffset(uint32_t value) noexcept { offset = value; }
    void setKey(uint32_t value) noexcept { key = value; }
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


template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheLine {
    using CacheAddress = BasicCacheAddress<offsetBits, tagBits, bankBits>;
    static constexpr auto NumberOfDataBytes = pow2(offsetBits);
    static constexpr auto NumberOfWords = NumberOfDataBytes / sizeof(SplitWord16);
    inline void clear() noexcept {
        dest_.clear();
        flags_.reg = 0;
        for (auto& word : words) {
            word.full = 0;
        }
    }
    inline bool matches(CacheAddress other) const noexcept {
        return flags_.valid_ && (other.getKey() == dest_.getKey());
    }
    inline void sync() noexcept {
        if (flags_.valid_ && flags_.dirty_) {
            memoryWrite(dest_.getBackingStore(), reinterpret_cast<uint8_t*>(words), NumberOfDataBytes);
            flags_.dirty_ = false;
        }
    }
    inline void reset(CacheAddress newAddress, bool doNotLoadFromMemoryOnMiss) noexcept {
        sync();
        flags_.valid_ = true;
        flags_.dirty_ = false; // mark it implicitly as dirty
        dest_ = newAddress;
        dest_.setOffset(0);
        if (!doNotLoadFromMemoryOnMiss) {
            memoryRead(dest_.getBackingStore(), reinterpret_cast<uint8_t *>(words), NumberOfDataBytes);
            if constexpr (EnableCacheDebugging) {
                Serial.println(F("\tSwapped in the following words: "));
                for (byte i = 0; i < NumberOfWords; ++i) {
                    Serial.print(F("\t\t"));
                    Serial.print(i);
                    Serial.print(F(": 0x"));
                    Serial.println(words[i].full, HEX);
                }
            }
        }
    }
    void begin() noexcept {
        clear();
    }
    [[nodiscard]] const SplitWord16* getData() const noexcept {
        return words;
    }
    inline uint16_t getWord(byte offset) const noexcept {
        auto result = words[offset].getWholeValue();
        if constexpr (EnableCacheDebugging) {
            Serial.print(F("getWord("));
            Serial.print(offset);
            Serial.print(F("): 0x"));
            Serial.println(result, HEX);
        }
        return result;
    }
    inline void setWord(byte offset, uint16_t value, EnableStyle style) noexcept {
        if constexpr (EnableCacheDebugging) {
            Serial.print(F("setWord("));
            Serial.print(offset);
            Serial.print(F(", 0x"));
            Serial.print(value, HEX);
            Serial.print(F(", 0x"));
            Serial.print(static_cast<byte>(style), HEX);
            Serial.println(F(")"));
        }
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
private:
    CacheAddress dest_{0};
    union {
        uint8_t reg;
        struct {
            uint8_t dirty_ : 1;
            uint8_t valid_ : 1;
        };

    } flags_;
    SplitWord16 words[NumberOfWords];
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct BasicDataCacheSet {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = numberOfLines;
    inline void begin() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    inline auto& find(CacheAddress address, bool doNotLoadFromMemoryOnMiss = false) noexcept {
        for (byte i = 0; i < NumberOfLines; ++i) {
            auto& line = lines[i];
            if (line.matches(address) ) {
                return line;
            }
        }
        auto& target = lines[replacementIndex_];
        updateFlags();
        target.reset(address, doNotLoadFromMemoryOnMiss);
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
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, 2> {
    // use MRU instead of round robin
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 2;
    inline void begin() noexcept {
        replacementTarget_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    [[nodiscard]] inline auto& find(CacheAddress address, bool dontLoadOnMiss) noexcept {
        for (int i = 0; i < NumberOfLines; ++i) {
            if (auto& line = lines[i]; line.matches(address)) {
                updateFlags(i);
                return line;
            }
        }
        auto index = getTargetLine();
        auto& target = lines[index];
        updateFlags(index);
        target.reset(address, dontLoadOnMiss);
        return target;
    }
    [[nodiscard]] inline byte getTargetLine() const noexcept {
        return replacementTarget_;
    }
    inline void updateFlags(int index) noexcept {
        replacementTarget_ = index == 1 ? 0 : 1;
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
struct BasicDataCacheSet<offsetBits, tagBits, bankBits, 1> {
    using DataCacheLine = BasicDataCacheLine<offsetBits, tagBits, bankBits>;
    using CacheAddress = typename DataCacheLine::CacheAddress;
    static constexpr auto NumberOfLines = 1;
    inline void begin() noexcept {
        line.begin();
    }
    [[nodiscard]] inline auto& find(CacheAddress address, bool doNotLoadFromMemoryOnMiss) noexcept {
        if (!line.matches(address)){
            line.reset(address, doNotLoadFromMemoryOnMiss);
        }
        return line;
    }
    inline void clear() noexcept {
        line.clear();
    }
private:
    DataCacheLine line;
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct BasicDataCache {
    using DataCacheSet = BasicDataCacheSet<offsetBits, tagBits, bankBits, numberOfLines>;
    using DataCacheLine = typename DataCacheSet::DataCacheLine;
    using CacheAddress = typename DataCacheSet::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[nodiscard]] inline auto& find(CacheAddress address, bool doNotLoadFromMemoryOnMiss = false) noexcept {
        if constexpr (EnableCacheDebugging) {
            Serial.print(F("Address: 0x"));
            Serial.println(address.getBackingStore().getWholeValue(), HEX);
            Serial.print(F("\tTag: 0x"));
            Serial.println(address.getTag(), HEX);
        }
        return cache[address.getTag()].find(address, doNotLoadFromMemoryOnMiss);
    }
    inline void begin() noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        return reinterpret_cast<byte*>(cache);
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return sizeof(cache);
    }
private:
    DataCacheSet cache[NumberOfSets];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCache<offsetBits, tagBits, bankBits, 1> {
    using DataCacheSet = BasicDataCacheSet<offsetBits, tagBits, bankBits, 1>;
    using DataCacheLine = typename DataCacheSet::DataCacheLine;
    using CacheAddress = typename DataCacheSet::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[nodiscard]] inline auto& find(CacheAddress address, bool doNotLoadFromMemoryOnMiss = false) noexcept {
        auto& line = cache[address.getTag()];
        if (!line.matches(address)){
            line.reset(address, doNotLoadFromMemoryOnMiss);
        }
        return line;
    }
    inline void begin() noexcept {
        for (auto& set : cache) {
            set.begin();
        }
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        return reinterpret_cast<byte*>(cache);
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return sizeof(cache);
    }
private:
    DataCacheLine cache[NumberOfSets];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct BasicCacheReference {
    using Cache = BasicDataCache<offsetBits, tagBits, bankBits, numberOfLines>;
    using DataCacheLine = typename Cache::DataCacheLine;
    using CacheAddress = typename Cache::CacheAddress;
    void select() const noexcept {
        xmem::setMemoryBank(index_);
    }
    void begin(byte index) noexcept {
        Serial.print(F("Sizeof cache = "));
        Serial.println(sizeof(Cache));
        index_ = index;
        select();
        ptr_ = new Cache();
        ptr_->begin();
    }
    auto& find(CacheAddress addr, bool skipLoadingFromMainMemory = false) noexcept {
        // make sure we select this bank before we jump to the pointer
        select();
        return ptr_->find(addr, skipLoadingFromMainMemory);
    }
    void clear() noexcept {
        select();
        ptr_->clear();
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        select();
        return ptr_->asBuffer();
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        select();
        return ptr_->sizeOfBuffer();
    }
private:
    byte index_ = 0;
    Cache* ptr_ = nullptr;
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t numberOfLines>
struct BasicCacheReference<offsetBits, tagBits, 0, numberOfLines> {
    using Cache = BasicDataCache<offsetBits, tagBits, 0, numberOfLines>;
    using DataCacheLine = typename Cache::DataCacheLine;
    using CacheAddress = typename Cache::CacheAddress;
    void begin() noexcept {
        Serial.print(F("Sizeof cache = "));
        Serial.println(sizeof(Cache));
        ptr_.begin();
    }
    inline auto& find(CacheAddress addr, bool skipLoadingFromMainMemory = false) noexcept {
        // make sure we select this bank before we jump to the pointer
        return ptr_.find(addr, skipLoadingFromMainMemory);
    }
    void clear() noexcept {
        ptr_.clear();
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        return ptr_.asBuffer();
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return ptr_.sizeOfBuffer();
    }
private:
    Cache ptr_;
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct CachePool {
    using CacheReference = BasicCacheReference<offsetBits, tagBits, bankBits, numberOfLines>;
    using CacheAddress = typename CacheReference::CacheAddress;
    using CacheLine = typename CacheReference::DataCacheLine;
    static constexpr auto NumberOfBanks = pow2(bankBits);
    void begin(byte bankOffset) noexcept {
        bankOffset_ = bankOffset;
        for (int i = 0, j = bankOffset_; i < NumberOfBanks; ++i, ++j) {
            pool_[i].begin(j);
        }
    }
    inline auto& find(const SplitWord32& address, bool skipLoadingFromMainMemory = false) noexcept {
        CacheAddress addr(address);
        return pool_[addr.getBankIndex()].find(addr, skipLoadingFromMainMemory);
    }
    void clear()  {
        for (int i = 0; i < NumberOfBanks; ++i) {
            pool_[i].clear();
        }
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        return pool_[0].asBuffer();
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return pool_[0].sizeOfBuffer();
    }
private:
    byte bankOffset_ = 0;
    CacheReference pool_[NumberOfBanks];
};

template<uint8_t offsetBits, uint8_t tagBits, uint8_t numberOfLines>
struct CachePool<offsetBits, tagBits, 0, numberOfLines> {
    using CacheReference = BasicCacheReference<offsetBits, tagBits, 0, numberOfLines>;
    using CacheAddress = typename CacheReference::CacheAddress;
    using CacheLine = typename CacheReference::DataCacheLine;
    static constexpr auto NumberOfBanks = pow2(0);
    void begin(byte = 0) noexcept {
        pool_.begin();
    }
    inline auto& find(const SplitWord32& address, bool skipLoadingFromMainMemory = false) noexcept {
        CacheAddress addr(address);
        return pool_.find(addr, skipLoadingFromMainMemory);
    }
    void clear()  {
        pool_.clear();
    }
    [[nodiscard]] byte* asBuffer() noexcept {
        return pool_.asBuffer();
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return pool_.sizeOfBuffer();
    }
private:
    CacheReference pool_;
};
#if defined(TYPE103_BOARD) || defined(TYPE104_BOARD)
//using MemoryCache = BasicDataCache<4, 8, 0, 2>;
using MemoryCache = CachePool<4, 8, 0, 2>;
#elif defined(TYPE203_BOARD) || defined(TYPE200_BOARD)
template<uint8_t bankBitCount>
using Pool12WayBanked = CachePool<4, 7, bankBitCount, 12>; // 322 bytes per bank
template<uint8_t bankBitCount>
using Pool6WayBanked = CachePool<4, 8, bankBitCount, 6>; // 27392 bytes per bank used
template<uint8_t bankBitCount>
using Pool8WayBanked = CachePool<4, 7, bankBitCount, 8>; // 27392 bytes per bank used
template<uint8_t bankBitCount>
using Pool2WayBanked = CachePool<4, 9, bankBitCount, 2>; //
template<uint8_t bankBitCount>
using Pool1WayBanked = CachePool<4, 10, bankBitCount, 1>; //

constexpr auto NumberOfBankBits = 4;
constexpr auto NumberOfOffsetBits = 4;
constexpr auto NumberOfTagBits = 9;
constexpr auto NumberOfWays = 2;
using ConfigurableMemoryCache = CachePool<NumberOfOffsetBits, NumberOfTagBits, NumberOfBankBits, NumberOfWays>;

using OnChipMemoryCache = CachePool<4, 8, 0, 1>;

using MemoryCache = ConfigurableMemoryCache;
#else
#error "Please correctly define internal cache size for target board"
#endif

MemoryCache& getCache() noexcept;
void setupCache() noexcept;
#endif //CHIPSET_CACHE_H

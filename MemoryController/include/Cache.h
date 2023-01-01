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

#ifndef SXCHIPSET_TYPE103_CACHE_MEMORY_CONTROLLER_H__
#define SXCHIPSET_TYPE103_CACHE_MEMORY_CONTROLLER_H__
#include <Arduino.h>
#include "Types.h"
#include "xmem.h"
size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;

enum class EnableStyle : byte {
    Full16 = 0b00,
    Upper8 = 0b01,
    Lower8 = 0b10,
    Undefined = 0b11,
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
union BasicCacheAddress {
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = bankBits;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    BasicCacheAddress(uint32_t address) : backingStore_(address) { }
    BasicCacheAddress(SplitWord32 address) : backingStore_(address) { }
    BasicCacheAddress(const SplitWord32& address) : backingStore_(address) { }
    SplitWord32 backingStore_;
    struct {
        uint32_t offset : OffsetBitsCount;
        uint32_t tag : TagBitsCount;
        uint32_t bank : BankBitsCount;
        uint32_t key : KeyBitsCount;
    };
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct BasicDataCacheLine {
    using CacheAddress = BasicCacheAddress<offsetBits, tagBits, bankBits>;
    static constexpr auto NumberOfDataBytes = pow2(offsetBits);
    static constexpr auto NumberOfWords = NumberOfDataBytes / NumberOfDataBytes;
    static constexpr uint8_t WordMask = NumberOfWords - 1;
    inline uint16_t getWord(byte offset) const noexcept {
        return words[offset & WordMask].full;
    }
    inline void clear() noexcept {
        key_ = 0;
        flags_.whole = 0;
        for (int i = 0; i < NumberOfWords; ++i) {
            words[i].full = 0;
        }
    }
    inline bool matches(CacheAddress other) const noexcept {
        return flags_.lineIsValid() && (other.cacheAddress.key == key_);
    }
    inline void reset(CacheAddress newAddress) noexcept {
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
        uint32_t key_ : CacheAddress::KeyBitsCount;
        union {
            uint8_t whole;
            struct {
                uint8_t dirty_ : 1;
                uint8_t valid_ : 1;
            } lineFlags;
        } flags_;
        SplitWord16 words[NumberOfWords];


};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines = 4>
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
    template<bool useMostRecentlyUsedCheck = false>
    inline auto& find(CacheAddress address) noexcept {
        if constexpr (useMostRecentlyUsedCheck) {
            for (int i = 0; i < NumberOfLines; ++i) {
                auto& line = lines[i];
                if (line.matches(address)) {
                    // shift ahead by one to make sure we don't swap the current
                    // line out on next miss. It is a form of most recently used
                    if (i == replacementIndex_) {
                        updateFlags();
                    }
                    return line;
                }
            }
        } else {
            for (auto& line : lines) {
                if (line.matches(address)) {
                    return line;
                }
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
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct BasicDataCache {
    using DataCacheSet = BasicDataCacheSet<offsetBits, tagBits, bankBits, numberOfLines>;
    using CacheAddress = typename DataCacheSet::CacheAddress;
    static constexpr auto NumberOfSets = pow2(tagBits);
    inline void clear() noexcept {
        for (auto& set : cache) {
            set.clear();
        }
    }
    [[gnu::always_inline]] inline auto& find(CacheAddress address) noexcept {
        return cache[address.cacheAddress.tag].find(address);
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

template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct BasicCacheReference {
    using Cache = BasicDataCache<offsetBits, tagBits, bankBits, numberOfLines>;
    void select() {
        if (initialized_) {
            xmem::setMemoryBank(index_);
        }
    }
    void begin(byte index) noexcept {
        if (!initialized_) {
            initialized_ = true;
            index_ = index;
            select();
            ptr_ = new Cache();
            if (ptr_) {
                ptr_->begin();
            }
        }
    }
    bool valid() const noexcept { return initialized_ && ptr_; }
    Cache& get() const noexcept { return *ptr_; }
    private:
        byte index_ = 0;
        bool initialized_ = false;
        Cache* ptr_ = nullptr;
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits, uint8_t numberOfLines>
struct CachePool {
    using CacheReference = BasicCacheReference<offsetBits, tagBits, bankBits, numberOfLines>;
    static constexpr auto NumberOfBanks = pow2(bankBits);
    void begin(byte bankOffset) noexcept {
        if (!initialized_) {
            initialized_ = true;
            bankOffset_ = bankOffset;
            for (int i = 0, j = bankOffset_; i < NumberOfBanks; ++i, ++j) {
                pool_[i].begin(j);
            }
        }
    }
    private:
        bool initialized_ = false;
        byte bankOffset_ = 0;
        CacheReference pool_[NumberOfBanks];
};

#endif // end !defined(SXCHIPSET_TYPE103_CACHE_MEMORY_CONTROLLER_H__)

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
size_t memoryWrite(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
size_t memoryRead(SplitWord32 baseAddress, uint8_t* bytes, size_t count) noexcept;
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
union CacheAddress {
    static constexpr auto OffsetBitsCount = offsetBits;
    static constexpr auto TagBitsCount = tagBits;
    static constexpr auto BankBitsCount = bankBits;
    static constexpr auto KeyDifferential = OffsetBitsCount + TagBitsCount + BankBitsCount;
    static_assert(KeyDifferential < 32, "Number of tag bits is too high");
    static constexpr auto KeyBitsCount = (32 - KeyDifferential);
    CacheAddress(uint32_t address) : backingStore_(address) { }
    SplitWord32 backingStore_;
    struct {
        uint32_t offset : OffsetBitsCount;
        uint32_t tag : TagBitsCount;
        uint32_t bank : BankBitsCount;
        uint32_t key : KeyBitsCount;
    };
};
template<uint8_t offsetBits, uint8_t tagBits, uint8_t bankBits>
struct DataCacheLine {
    static constexpr auto NumberOfDataBytes = 
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
        uint32_t key_ : KeySize;
        Word8 flags_;
        SplitWord16 words[NumberOfWords];


};
struct DataCacheSet {
    static constexpr auto NumberOfLines = 4;
    inline void begin() noexcept {
        replacementIndex_ = 0;
        for (auto& line : lines) {
            line.begin();
        }
    }
    template<bool useMostRecentlyUsedCheck = false>
    inline auto& find(SplitWord32 address) noexcept {
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
struct DataCache {
    static constexpr auto NumberOfSets = getNumberOfSets();
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
    [[nodiscard]] byte* asBuffer() noexcept {
        return reinterpret_cast<byte*>(cache);
    }
    [[nodiscard]] constexpr size_t sizeOfBuffer() const noexcept {
        return sizeof(cache);
    }
    private:
        DataCacheSet cache[NumberOfSets];
};
DataCache& getCache(const SplitWord32& address) noexcept;
void setupCache() noexcept;

#endif // end !defined(SXCHIPSET_TYPE103_CACHE_MEMORY_CONTROLLER_H__)
